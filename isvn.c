/*
 * Builtin "git isvn"
 *
 * Clone an SVN repository into a different directory that does not yet exist.
 *
 * Copyright (c) 2014 Conrad Meyer <cse.cem@gmail.com>
 * Based on builtin/clone.c by Kristian Høgsberg and Daniel Barkalow,
 *          git.c by Linus Torvalds, Junio Hamano, and many others.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>

#include <dirent.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <pthread.h>

#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_hash.h>
#include <svn_path.h>
#include <svn_props.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_repos.h>
#include <svn_string.h>

#include "isvn/isvn-git2.h"
#include "isvn/isvn-internal.h"

static const char *cmd_isvn_usage[] = {
	"git isvn clone [options] [--] <repo> <dir>",
	NULL
};
static const char *isvn_clone_usage[] = {
	"git isvn clone [options] [--] <repo> <dir>",
	NULL
};

static bool option_cloning;  /* empty repo */
static int option_maxrev;

/* XXX */
int option_verbosity;
char *option_origin = NULL;
const char *option_trunk = "head";
const char *option_branches = "branches";
const char *option_tags = "tags";
const char *option_user, *option_password;
const char *g_svn_url;
const char *g_repos_root;
git_repository *g_git_repo;
bool option_debugging;

unsigned g_nr_fetch_workers = 1;
/* XXX Restrict to 1 brancher until libgit2 has multi-index. */
unsigned g_nr_commit_workers = 1;
unsigned g_rev_chunk = 25;

apr_pool_t *g_apr_pool;			/* (u) */

/*
 * Locking:
 *   (u) - Unlocked, not modified during threads
 *   (g) - g_lock
 */

static unsigned g_rev_low, g_rev_high;		/* (u) */
static bool g_locks_initted = false;		/* (u) */
static pthread_t *g_fetch_workers,		/* (u) */
		 *g_branch_workers;		/* (u) */

static pthread_mutex_t g_lock;
static unsigned g_rev_lo_avail;			/* (g) */
static pthread_cond_t g_rev_cond;		/* (g) */
static unsigned g_rev_fetchdone;		/* (g) */

struct fetchdone_range {
	struct hashmap_entry r_entry;
	unsigned r_lo,	/* key */
		 r_hi;
};

static int
fetchdone_range_cmp(const struct fetchdone_range *r1,
	const struct fetchdone_range *r2, const void *dummy)
{
	return (int)r1->r_lo - (int)r2->r_lo;
}

static struct hashmap g_fetchdone_hash;		/* (g) */

static unsigned g_rev_commitdone;		/* (g) */
static struct hashmap g_commitdone_hash;	/* (g) */
static struct hashmap g_commitdrain_hash;	/* (g) */

void
isvn_fetcher_getrange(unsigned *revlo, unsigned *revhi, bool *done)
{
	*done = false;

	isvn_g_lock();
	*revlo = g_rev_lo_avail;
	*revhi = *revlo + g_rev_chunk - 1;

	if (*revlo > g_rev_high)
		*done = true;
	if (*revhi > g_rev_high)
		*revhi = g_rev_high;

	g_rev_lo_avail = *revhi + 1;
	isvn_g_unlock();
}

void
isvn_mark_fetchdone(unsigned revlo, unsigned revhi)
{
	struct fetchdone_range *done, *exist, key;

	done = xmalloc(sizeof(*done));
	if (done == NULL)
		die("malloc");

	isvn_g_lock();
	if (g_rev_fetchdone == revlo - 1) {
		g_rev_fetchdone = revhi;

		while (true) {
			key.r_lo = revhi + 1;
			hashmap_entry_init(&key.r_entry,
				memhash(&key.r_lo, sizeof(key.r_lo)));

			exist = hashmap_remove(&g_fetchdone_hash, &key, NULL);
			if (!exist)
				break;

			g_rev_fetchdone = revhi = exist->r_hi;
			free(exist);
		}

		cond_broadcast(&g_rev_cond);
	} else {
		done->r_lo = revlo;
		done->r_hi = revhi;
		hashmap_entry_init(&done->r_entry,
			memhash(&done->r_lo, sizeof(done->r_lo)));
		hashmap_add(&g_fetchdone_hash, done);
		done = NULL;
	}
	isvn_g_unlock();

	if (done)
		free(done);
}

/* Blocks until all revs up to 'rev' are fetched. */
void
isvn_wait_fetch(unsigned rev)
{
	isvn_g_lock();
	while (rev > g_rev_fetchdone)
		cond_wait(&g_rev_cond, &g_lock);
	isvn_g_unlock();
}

bool
isvn_all_fetched(void)
{
	bool ret;

	isvn_g_lock();
	ret = (g_rev_fetchdone >= g_rev_high);
	isvn_g_unlock();

	return ret;
}

void
_isvn_commitdrain_add(unsigned rev, int incr)
{
	struct fetchdone_range *newr, *exist, key;

	key.r_lo = rev;
	hashmap_entry_init(&key.r_entry, memhash(&key.r_lo, sizeof(key.r_lo)));

	if (incr > 0) {
		newr = xmalloc(sizeof(*newr));
		newr->r_lo = rev;
		newr->r_hi = incr;  /* reused as refcnt */
		hashmap_entry_init(&newr->r_entry,
		    memhash(&newr->r_lo, sizeof(newr->r_lo)));
	} else
		newr = NULL;

	isvn_g_lock();
	exist = hashmap_get(&g_commitdrain_hash, &key, NULL);

	if (incr > 0) {
		if (exist)
			exist->r_hi += incr;
		else
			hashmap_add(&g_commitdrain_hash, newr);
	} else {
		int refcnt;

		/* INVARIANTS */
		if (exist == NULL)
			die("negative refcnt %d (ne)", incr);

		refcnt = (int)exist->r_hi + incr;

		/* INVARIANTS */
		if (refcnt < 0)
			die("negative refcnt %d", refcnt);

		if (refcnt > 0)
			exist->r_hi = refcnt;
		else {
			hashmap_remove(&g_commitdrain_hash, exist, NULL);
			/* free it below */
			newr = exist;
		}
	}
	isvn_g_unlock();

	if (exist && newr)
		free(newr);
}

void
isvn_mark_commitdone(unsigned revlo, unsigned revhi)
{
	struct fetchdone_range *done, *exist, key;
	struct fetchdone_range *drainex, drainkey;

	/* In particular, checking for refs in need of draining ... for each
	 * rev in range. */
	if (revlo != revhi)
		die("XXX batched commitdones notimpl.");

	done = xmalloc(sizeof(*done));

	drainkey.r_lo = revlo;
	hashmap_entry_init(&drainkey.r_entry,
	    memhash(&drainkey.r_lo, sizeof(drainkey.r_lo)));

	isvn_g_lock();

	/* For revs with multiple branch edits (rare), wait until all commits
	 * are in before marking done. */
	drainex = hashmap_get(&g_commitdrain_hash, &drainkey, NULL);
	if (drainex) {
		drainex->r_hi--;
		if (drainex->r_hi == 0) {
			hashmap_remove(&g_commitdrain_hash, drainex, NULL);
			free(drainex);
		} else
			goto out;
	}

	if (g_rev_commitdone == revlo - 1) {
		g_rev_commitdone = revhi;

		while (true) {
			key.r_lo = revhi + 1;
			hashmap_entry_init(&key.r_entry,
				memhash(&key.r_lo, sizeof(key.r_lo)));

			exist = hashmap_remove(&g_commitdone_hash, &key, NULL);
			if (!exist)
				break;

			g_rev_commitdone = revhi = exist->r_hi;
			free(exist);
		}
	} else {
		done->r_lo = revlo;
		done->r_hi = revhi;
		hashmap_entry_init(&done->r_entry,
			memhash(&done->r_lo, sizeof(done->r_lo)));
		hashmap_add(&g_commitdone_hash, done);
		done = NULL;
	}

out:
	isvn_g_unlock();

	if (done)
		free(done);
}

bool
isvn_has_commit(unsigned rev)
{
	bool ret;

	isvn_g_lock();
	ret = (g_rev_commitdone >= rev);
	isvn_g_unlock();

	return ret;
}

void
isvn_assert_commit(const char *branch, unsigned rev)
{

	if (!isvn_has_commit(rev))
		die("%s: %s@%u missing", __func__, branch, rev);
}

void
isvn_commitdone_dump(void)
{

	printf("\tcommitdone: r%u\n", g_rev_commitdone);
	printf("\tcommits pending: %zu\n", g_commitdone_hash.hm_size);
	/* XXX */
}

void
isvn_g_lock(void)
{
	mtx_lock(&g_lock);
}

void
isvn_g_unlock(void)
{
	mtx_unlock(&g_lock);
}

static void
isvn_globals_init(void)
{
	mtx_init(&g_lock);
	cond_init(&g_rev_cond);

	g_locks_initted = true;

	/* Override defaults (noops) w/ our edit CBs */
	assert_status_noerr(apr_pool_create(&g_apr_pool, NULL),
	    "apr_pool_create");

	hashmap_init(&g_fetchdone_hash, (hashmap_cmp_fn)fetchdone_range_cmp, 0);
	hashmap_init(&g_commitdone_hash, (hashmap_cmp_fn)fetchdone_range_cmp, 0);
	hashmap_init(&g_commitdrain_hash, (hashmap_cmp_fn)fetchdone_range_cmp, 0);

	g_fetch_workers = xcalloc(g_nr_fetch_workers, sizeof(*g_fetch_workers));
	g_branch_workers = xcalloc(g_nr_commit_workers, sizeof(*g_branch_workers));

	if (option_verbosity >= 3)
		printf("Initialized with %u fetchers, %u committers.\n",
		    g_nr_fetch_workers, g_nr_commit_workers);
}

static int
isvn_fetch(struct isvn_client_ctx *ctx)
{
	unsigned i;
	int rc;

	isvn_globals_init();
	isvn_revmap_init();
	isvn_fetch_init();
	isvn_brancher_init();
	isvn_git_compat_init();

	assert_noerr(
		svn_ra_get_repos_root2(ctx->svn_session, &g_repos_root,
			ctx->svn_pool),
		"svn_ra_get_repos_root2");

	if (option_verbosity >= 2)
		printf("SVN root is: %s\n", g_repos_root);

	isvn_editor_init();

	/* 1. Determine g_rev_low/g_rev_high. */
	if (option_cloning)
		g_rev_low = g_rev_lo_avail = 1;
	else
		die("low rev ???");  /* Determine. Maybe store as a ?bogus ref */

	g_rev_fetchdone = g_rev_commitdone = g_rev_low - 1;
	g_rev_high = option_maxrev;

	/* 2. Spawn fetchers. */
	for (i = 0; i < g_nr_fetch_workers; i++) {
		rc = pthread_create(&g_fetch_workers[i], NULL,
			isvn_fetch_worker, (void *)(uintptr_t)i);
		if (rc)
			die("pthread_create: %s(%d)\n", strerror(rc), rc);
	}

	/* 3. Spawn bucket workers. */
	for (i = 0; i < g_nr_commit_workers; i++) {
		rc = pthread_create(&g_branch_workers[i], NULL,
			isvn_bucket_worker, (void *)(uintptr_t)i);
		if (rc)
			die("pthread_create: %s(%d)\n", strerror(rc), rc);
	}

	/* 4. Wind it down. */
	for (i = 0; i < g_nr_fetch_workers; i++) {
		rc = pthread_join(g_fetch_workers[i], NULL);
		if (rc)
			die("pthread_join: %s(%d)\n", strerror(rc), rc);
	}

	for (i = 0; i < g_nr_commit_workers; i++) {
		rc = pthread_join(g_branch_workers[i], NULL);
		if (rc)
			die("pthread_join: %s(%d)\n", strerror(rc), rc);
	}

	return 0;
}

/* ************************** 'git isvn clone' **************************** */

static struct usage_option isvn_clone_options[] = {
	{ 'v', "verbose", "be more verbose", no_argument, &option_verbosity },
	{ 'D', "debug", "crash on failure", no_argument, &option_debugging },

	{ 'o', "origin", "use <name> instead of 'origin' to track upstream",
		required_argument, &option_origin },
	{ 'b', "branches", "use <branches>/* for upstream SVN branches",
		required_argument, &option_branches },
	{ 't', "trunk", "use <trunk> as the SVN master branch",
		required_argument, &option_trunk },
	{ 'r', "maxrev", "use as the max SVN revision to fetch",
		required_argument, &option_maxrev },
	{ 'u', "username", "use <user> for SVN authentication",
		required_argument, &option_user },
	{ 'p', "password", "use <password> for SVN authentication",
		required_argument, &option_password },

	{ 'R', "rev-chunk", "fetch <N> revisions in a batch",
		required_argument, &g_rev_chunk },
	{ 'F', "fetch-workers", "Use <N> threads to fetch from SVN",
		required_argument, &g_nr_fetch_workers },
	{ 'C', "commit-workers", "Use <N> threads to commit to Git",
		required_argument, &g_nr_commit_workers },
	{ 0 }
};

static void
usage_to_getopt_options(const struct usage_option *in, struct option **out,
    char **sout)
{
	struct option *res;
	unsigned nopts;
	char *sopt, *s;

	for (nopts = 0; in[nopts].flag; nopts++)
		/* just counting. */;

	/* Extra for --help, NULL */
	res = xcalloc(nopts + 2, sizeof(*res));

	/* For each short flag: "f::" = 3, + "h\0" */
	sopt = xcalloc(1, nopts * 3 + 2);

	s = sopt;
	for (nopts = 0; in[nopts].flag; nopts++) {
		res[nopts].name = in[nopts].longname;
		res[nopts].has_arg = in[nopts].has_arg;
		res[nopts].flag = NULL;
		res[nopts].val = in[nopts].flag;

		*s++ = in[nopts].flag;
		if (in[nopts].has_arg == required_argument)
			*s++ = ':';
		else if (in[nopts].has_arg == optional_argument) {
			*s++ = ':';
			*s++ = ':';
		}
	}

	res[nopts].name = "help";
	res[nopts].has_arg = no_argument;
	res[nopts].flag = NULL;
	res[nopts].val = 'h';
	*s++ = 'h';

	nopts++;
	res[nopts].name = NULL;
	res[nopts].has_arg = 0;
	res[nopts].flag = NULL;
	res[nopts].val = 0;

	*s++ = '\0';

	*sout = sopt;
	*out = res;
}

static char *
guess_dir_name(const char *repo_name)
{
	const char *lastslash;
	char *dir;

	lastslash = strrchr(repo_name, '/');
	if (lastslash && lastslash[1] == '\0')
		lastslash = memrchr(repo_name, '/', lastslash - repo_name - 1);
	if (lastslash == NULL)
		die("bogus repo");

	dir = xstrdup(lastslash);
	if (strchr(dir, '/'))
		*strchr(dir, '/') = '\0';

	return dir;
}

static void
checkout(git_repository *repo)
{
	git_reference *master_ref, *remote_ref, *HEAD_ref, *tmpr;
	git_checkout_options co_opts;
	const char *remote_br_name;
	git_commit *remote_commit;
	git_object *remote_obj;
	char *remotebr;
	int rc;

	remotebr = NULL;
	xasprintf(&remotebr, "refs/remotes/%s/%s", option_origin,
	    option_trunk);

	rc = git_reference_lookup(&remote_ref, repo, remotebr);
	if (rc)
		die("git_reference_lookup(%s): %d", remotebr, rc);

	rc = git_branch_name(&remote_br_name, remote_ref);
	if (rc)
		die("git_branch_name: %d", rc);

	rc = git_reference_peel(&remote_obj, remote_ref, GIT_OBJ_COMMIT);
	if (rc)
		die("git_reference_peel");

	rc = git_commit_lookup(&remote_commit, repo,
	    git_object_id(remote_obj));
	if (rc)
		die("git_commit_lookup");

	rc = git_branch_create(&master_ref, repo, "master", remote_commit,
	    false, NULL, NULL);
	if (rc)
		die("git_branch_create: %d", rc);

	rc = git_reference_symbolic_create(&HEAD_ref, repo, "HEAD",
	    "refs/heads/master", false, NULL, NULL);
	if (rc && rc != GIT_EEXISTS)
		die("git_reference_symbolic_create: %d", rc);

	rc = git_branch_set_upstream(master_ref, remote_br_name);
	if (rc)
		/* TODO: '' is not a valid remote name */
#if 0
		die("git_branch_set_upstream: %d (%d/%s)", rc,
		    giterr_last()->klass, giterr_last()->message);
#else
		printf("XXXgit_branch_set_upstream: %d (%d/%s)\n", rc,
		    giterr_last()->klass, giterr_last()->message);
#endif

	rc = git_reference_lookup(&tmpr, repo, "refs/heads/master");
	if (rc)
		die("%s: reference_lookup: master doesn't exist?", __func__);
	if (git_reference_cmp(tmpr, master_ref) != 0)
		die("mismatched master");

	co_opts = (git_checkout_options) GIT_CHECKOUT_OPTIONS_INIT;
	co_opts.checkout_strategy = GIT_CHECKOUT_SAFE_CREATE;
	rc = git_checkout_head(repo, &co_opts);
	if (rc)
		die("git_checkout_head");

	free(remotebr);
	git_commit_free(remote_commit);
	git_object_free(remote_obj);
	git_reference_free(tmpr);
	git_reference_free(HEAD_ref);
	git_reference_free(master_ref);
	git_reference_free(remote_ref);
}

static void __attribute__((noreturn))
isvn_usage(const char **usages, const struct usage_option *opts,
    const char *auxmsg)
{
	unsigned i;

	if (auxmsg)
		printf("Error: %s\n", auxmsg);

	printf("Usage:\n");
	for (; *usages; usages++)
		printf("\t%s\n", *usages);

	if (opts) {
		for (i = 0; opts[i].flag; i++)
			printf("\t-%c, --%s:\t%s\n", opts[i].flag,
			    opts[i].longname, opts[i].usage);
	}

	exit(0);
}

static long
isvn_opt_parse_long(const char **us, const struct usage_option *lopts,
    const char *str, const char *failmsg)
{
	char *ep;
	long res;

	errno = 0;
	res = strtol(str, &ep, 0);
	if (errno || *ep || res < 0)
		isvn_usage(us, lopts, failmsg);

	return res;
}

/* Could be more general (other isvn commands?) but it doesn't have to, yet. */
static int
isvn_parse_opts(int ac, char **av, const struct usage_option *lopts,
    const char **usages)
{
	struct option *gopts;
	char *sopts;
	int c, idx;

	usage_to_getopt_options(lopts, &gopts, &sopts);

	while ((c = getopt_long(ac, av, sopts, gopts, &idx)) != -1) {
		switch (c) {
		case '?':
		default:
			isvn_usage(usages, lopts, "Unrecognized or bad option");
			/* NORETURN */

		case 'h':
			isvn_usage(usages, lopts, NULL);
			/* NORETURN */

		case 'v':
			option_verbosity++;
			break;

		case 'D':
			fprintf(stderr, "Developer debugging mode enabled.\n");
			option_debugging = true;
			break;

		case 'b':
		case 'o':
		case 'p':
		case 't':
		case 'u':
			*(char **)lopts[idx].extra = xstrdup(optarg);
			break;

		case 'r':
			option_maxrev = isvn_opt_parse_long(usages, lopts,
			    optarg, "Bad revision");
			break;

		case 'C':
			fprintf(stderr,
			    "Cannot support >1 commit thread at this time. libgit2 does not support "
			    "using multiple independent indices (yet).\n");
			exit(EX_SOFTWARE);
			/* NORETURN */
			/* After libgit2 / multiple indices implemented, this
			 * will be a FALLTHROUGH. */
		case 'F':
		case 'R':
			*(unsigned *)lopts[idx].extra =
			    (unsigned)isvn_opt_parse_long(usages, lopts,
				optarg, "Bad quantity");
			break;
		}
	}

	free(gopts);
	free(sopts);

	memmove(av, &av[optind], (ac - optind + 1) * sizeof(av[0]));
	return (ac - optind);
}

static int cmd_isvn_clone(int argc, const char **cargv, const char *prefix)
{
	svn_revnum_t latest_rev = SVN_INVALID_REVNUM;
	struct isvn_client_ctx *first_client;
	char *dir, *key, **argv;
	const char *repo_name;
	svn_error_t *err;

	git_repository_init_options initopts;
	git_config *config;
	int rc;

	argv = __DECONST(cargv, char **);
	argc = isvn_parse_opts(argc, argv, isvn_clone_options,
	    isvn_clone_usage);

	if (argc < 1)
		isvn_usage(isvn_clone_usage, isvn_clone_options,
		    "You must specify a repository to clone.");
	else if (argc < 2)
		isvn_usage(isvn_clone_usage, isvn_clone_options,
		    "You must specify a destination directory.");
	else if (argc > 2)
		isvn_usage(isvn_clone_usage, isvn_clone_options,
		    "Too many arguments.");

	if (!option_origin)
		option_origin = "origin";
	option_cloning = true;
	if (option_maxrev < 0)
		isvn_usage(isvn_clone_usage, isvn_clone_options,
		    "Maxrev cannot be negative");

	repo_name = argv[0];
	if (argc == 2)
		dir = xstrdup(argv[1]);
	else
		dir = guess_dir_name(repo_name);

	initopts = (git_repository_init_options) GIT_REPOSITORY_INIT_OPTIONS_INIT;
	initopts.flags = GIT_REPOSITORY_INIT_MKDIR |
	    GIT_REPOSITORY_INIT_NO_REINIT |
	    GIT_REPOSITORY_INIT_MKPATH;

	rc = git_repository_init_ext(&g_git_repo, dir, &initopts);
	if (rc < 0)
		die("Error creating git repository: %d", rc);

	rc = git_repository_config(&config, g_git_repo);
	if (rc)
		die("git_repository_config: %d", rc);

	xasprintf(&key, "remote.%s.url", option_origin);
	rc = git_config_set_string(config, key, repo_name);
	if (rc)
		die("git_config_set_string: %d", rc);
	g_svn_url = xstrdup(repo_name);
	free(key);

	/* Initialize APR / libsvn */
	if (svn_cmdline_init("git-isvn", stderr) != EXIT_SUCCESS)
		die("svn_cmdline_init");

	first_client = get_svn_ctx();
	if (first_client == NULL)
		die("Could not connect to SVN server.");

	err = svn_ra_get_latest_revnum(first_client->svn_session, &latest_rev,
		first_client->svn_pool);
	assert_noerr(err, "svn_ra_get_latest_revnum");

	if (option_verbosity >= 2)
		printf("Maximum SVN repository revision: %ju\n",
		    (uintmax_t)latest_rev);

	if (option_maxrev == 0 || option_maxrev > latest_rev)
		option_maxrev = latest_rev;

	isvn_fetch(first_client);

	/* Branch 'master' from remote trunk and checkout. */
	checkout(g_git_repo);

	git_config_free(config);
	git_repository_free(g_git_repo);
	g_git_repo = NULL;

	free(dir);
	return 0;
}

/* ************************** 'git isvn' **************************** */

static struct {
	const char *sub_name;
	int (*sub_cmd)(int, const char **, const char *);
} isvn_subcommands[] = {
	{ "clone", cmd_isvn_clone },
};

int
main(int argc, const char **argv)
{
	unsigned i;
	int rc;

	rc = git_threads_init();
	if (rc < 0)
		die("git_threads_init: %d", rc);

	if ((git_libgit2_features() & GIT_FEATURE_THREADS) == 0)
		die("libgit2: no threads???");

	/* Skip over 'isvn' */
	argc--;
	argv++;
	if (argc == 0)
		isvn_usage(cmd_isvn_usage, NULL, NULL);	/* NORETURN */

	/* Dispatch subcommands ... */
	for (i = 0; i < ARRAY_SIZE(isvn_subcommands); i++) {
		if (strcmp(argv[0], isvn_subcommands[i].sub_name) == 0) {
			isvn_subcommands[i].sub_cmd(argc, argv, NULL);
			return 0;
		}
	}

	printf("No such command `%s'\n", argv[0]);
	isvn_usage(cmd_isvn_usage, NULL, NULL);	/* NORETURN */
}
