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

#include <stdbool.h>
#include <stdio.h>
#include <sysexits.h>
#include <unistd.h>

#include <svn_client.h>
#include <svn_cmdline.h>
#include <svn_hash.h>
#include <svn_path.h>
#include <svn_props.h>
#include <svn_pools.h>
#include <svn_ra.h>
#include <svn_repos.h>
#include <svn_string.h>

#define NO_THE_INDEX_COMPATIBILITY_MACROS

#include "builtin.h"

#include "branch.h"
#include "dir.h"
#include "help.h"
#include "parse-options.h"
#include "remote.h"
#include "thread-utils.h"
#include "transport.h"
#include "tree-walk.h"
#include "unpack-trees.h"

#include "isvn/isvn-internal.h"

static const char *builtin_isvn_usage[] = {
	"git isvn clone [options] [--] <repo> <dir>",
	NULL
};
static const char *builtin_isvn_clone_usage[] = {
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

apr_pool_t *g_apr_pool;			/* (u) */

/*
 * Locking:
 *   (u) - Unlocked, not modified during threads
 *   (g) - g_lock
 */

static unsigned g_rev_low, g_rev_high;		/* (u) */
static bool g_locks_initted = false;		/* (u) */
static pthread_t g_fetch_workers[NR_WORKERS],	/* (u) */
		 g_branch_workers[NR_WORKERS];	/* (u) */

static pthread_mutex_t g_lock;
static unsigned g_rev_lo_avail;			/* (g) */
static pthread_cond_t g_rev_cond;		/* (g) */
static unsigned g_rev_fetchdone;		/* (g) */

struct fetchdone_range {
	struct hashmap_entry r_entry;
	unsigned r_lo,	/* key */
		 r_hi;
};
static int fetchdone_range_cmp(const struct fetchdone_range *r1,
	const struct fetchdone_range *r2, const void *dummy)
{
	return (int)r1->r_lo - (int)r2->r_lo;
}
static struct hashmap g_fetchdone_hash;		/* (g) */

void isvn_fetcher_getrange(unsigned *revlo, unsigned *revhi, bool *done)
{
	*done = false;

	isvn_g_lock();
	*revlo = g_rev_lo_avail;
	*revhi = *revlo + REV_CHUNK - 1;

	if (*revlo > g_rev_high)
		*done = true;
	if (*revhi > g_rev_high)
		*revhi = g_rev_high;

	g_rev_lo_avail = *revhi + 1;
	isvn_g_unlock();
}

void isvn_mark_fetchdone(unsigned revlo, unsigned revhi)
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
void isvn_wait_fetch(unsigned rev)
{
	isvn_g_lock();
	while (rev > g_rev_fetchdone)
		cond_wait(&g_rev_cond, &g_lock);
	isvn_g_unlock();
}

bool isvn_all_fetched(void)
{
	bool ret;

	isvn_g_lock();
	ret = (g_rev_fetchdone >= g_rev_high);
	isvn_g_unlock();

	return ret;
}

void isvn_g_lock(void)
{
	mtx_lock(&g_lock);
}

void isvn_g_unlock(void)
{
	mtx_unlock(&g_lock);
}

static void isvn_globals_init(void)
{
	mtx_init(&g_lock);
	cond_init(&g_rev_cond);

	g_locks_initted = true;

	/* Override defaults (noops) w/ our edit CBs */
	assert_status_noerr(apr_pool_create(&g_apr_pool, NULL),
		"apr_pool_create");

	hashmap_init(&g_fetchdone_hash, (hashmap_cmp_fn)fetchdone_range_cmp,
		0);
}

static int isvn_fetch(struct isvn_client_ctx *ctx, struct remote *remote)
{
	unsigned i;
	int rc;

	isvn_globals_init();
	isvn_revmap_init();
	isvn_fetch_init();
	isvn_brancher_init();

	assert_noerr(
		svn_ra_get_repos_root2(ctx->svn_session, &g_repos_root,
			ctx->svn_pool),
		"svn_ra_get_repos_root2");
	printf("XXX root: %s\n", g_repos_root);

	isvn_editor_init();

	/* 1. Determine g_rev_low/g_rev_high. */
	if (option_cloning)
		g_rev_low = g_rev_lo_avail = 1;
	else
		die("low rev ???");  /* Determine. Maybe store as a ?bogus ref */

	g_rev_fetchdone = g_rev_low - 1;
	g_rev_high = option_maxrev;

	/* 2. Spawn fetchers. */
	for (i = 0; i < NR_WORKERS; i++) {
		rc = pthread_create(&g_fetch_workers[i], NULL,
			isvn_fetch_worker, (void *)(uintptr_t)i);
		if (rc)
			die("pthread_create: %s(%d)\n", strerror(rc), rc);
	}

	/* 3. Spawn bucket workers. */
	for (i = 0; i < NR_WORKERS; i++) {
		rc = pthread_create(&g_branch_workers[i], NULL,
			isvn_bucket_worker, (void *)(uintptr_t)i);
		if (rc)
			die("pthread_create: %s(%d)\n", strerror(rc), rc);
	}

	/* 4. Wind it down. */
	for (i = 0; i < NR_WORKERS; i++) {
		rc = pthread_join(g_fetch_workers[i], NULL);
		if (rc)
			die("pthread_join: %s(%d)\n", strerror(rc), rc);
	}

	for (i = 0; i < NR_WORKERS; i++) {
		rc = pthread_join(g_branch_workers[i], NULL);
		if (rc)
			die("pthread_join: %s(%d)\n", strerror(rc), rc);
	}

	return 0;
}

/* ************************** 'git isvn clone' **************************** */

static struct option builtin_isvn_clone_options[] = {
	OPT__VERBOSITY(&option_verbosity),
	OPT_STRING('o', "origin", &option_origin, N_("name"),
		   N_("use <name> instead of 'origin' to track upstream")),
	OPT_STRING('b', "branches", &option_branches, N_("branches"),
		   N_("use <branches>/* for upstream SVN branches")),
	OPT_STRING('t', "trunk", &option_trunk, N_("trunk"),
		   N_("use <trunk> as the SVN master branch")),
	OPT_INTEGER('r', "maxrev", &option_maxrev,
		   N_("use as the max SVN revision to fetch")),
	OPT_STRING('u', "username", &option_user, N_("user"),
		   N_("use <user> for SVN authentication")),
	OPT_STRING('p', "password", &option_password, N_("password"),
		   N_("use <password> for SVN authentication")),
	OPT_END()
};

static char *guess_dir_name(const char *repo_name)
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

static void checkout(void)
{
	struct strbuf remotebr = STRBUF_INIT;
	struct unpack_trees_options opts;
	struct lock_file *lock_file;
	unsigned char sha1[20];
	struct tree_desc t;
	struct tree *tree;
	int rc;

	strbuf_addf(&remotebr, "refs/remotes/%s/%s", option_origin,
		option_trunk);

	create_branch(NULL, "master", remotebr.buf, false, true, false,
		(option_verbosity < 0), git_branch_track);

	strbuf_release(&remotebr);

	create_symref("HEAD", "refs/heads/master", NULL);
	install_branch_config((option_verbosity >= 0)? BRANCH_CONFIG_VERBOSE : 0,
		"master", option_origin, option_trunk);

	setup_work_tree();
	rc = read_ref("refs/heads/master", sha1);
	if (rc < 0)
		die("%s: read_ref: master doesn't exist?", __func__);

	lock_file = xcalloc(1, sizeof(struct lock_file));
	hold_locked_index(lock_file, 1);

	memset(&opts, 0, sizeof opts);
	opts.update = 1;
	opts.merge = 1;
	opts.fn = oneway_merge;
	opts.verbose_update = (option_verbosity >= 0);
	opts.src_index = &the_index;
	opts.dst_index = &the_index;

	tree = parse_tree_indirect(sha1);
	parse_tree(tree);
	init_tree_desc(&t, tree->buffer, tree->size);

	rc = unpack_trees(1, &t, &opts);
	if (rc < 0)
		die(_("unable to checkout working tree"));

	rc = write_locked_index(&the_index, lock_file, COMMIT_LOCK);
	if (rc)
		die(_("unable to write new index file"));
}

static int cmd_isvn_clone(int argc, const char **argv, const char *prefix)
{
	svn_revnum_t latest_rev = SVN_INVALID_REVNUM;
	struct isvn_client_ctx *first_client;
	struct strbuf key = STRBUF_INIT;
	const char *repo_name, *git_dir;
	struct remote *remote;
	struct stat stbuf;
	svn_error_t *err;
	char *dir;

	argc = parse_options(argc, argv, prefix, builtin_isvn_clone_options,
		builtin_isvn_clone_usage, 0);

	if (argc < 1)
		usage_msg_opt(_("You must specify a repository to clone."),
			builtin_isvn_clone_usage, builtin_isvn_clone_options);
	else if (argc < 2)
		usage_msg_opt(_("You must specify a destination directory."),
			builtin_isvn_clone_usage, builtin_isvn_clone_options);
	else if (argc > 2)
		usage_msg_opt(_("Too many arguments."),
			builtin_isvn_clone_usage, builtin_isvn_clone_options);

	if (!option_origin)
		option_origin = "origin";
	option_cloning = true;
	if (option_maxrev < 0)
		usage_msg_opt(_("Maxrev cannot be negative"),
			builtin_isvn_clone_usage, builtin_isvn_clone_options);

	repo_name = argv[0];
	if (argc == 2)
		dir = xstrdup(argv[1]);
	else
		dir = guess_dir_name(repo_name);

	if (stat(dir, &stbuf) == 0 && !is_empty_dir(dir)) {
		die(_("destination path '%s' already exists and is not "
			"an empty directory."), dir);
		/* NORETURN */
	}

	if (0 <= option_verbosity)
		fprintf(stderr, _("Cloning into '%s'...\n"), dir);

	git_dir = mkpathdup("%s/.git", dir);
	if (safe_create_leading_directories_const(dir) < 0)
		die_errno(_("could not create leading directories of '%s'"),
			dir);
		/* NORETURN */
	if (mkdir(dir, 0777) == -1 && errno != EEXIST)
		die_errno(_("could not create work tree dir '%s'."),
			dir);
		/* NORETURN */
	set_git_work_tree(dir);
	set_git_dir_init(git_dir, NULL, 0);

	init_db(NULL, INIT_DB_QUIET);
	git_config(git_default_config, NULL);

	strbuf_addf(&key, "remote.%s.url", option_origin);
	git_config_set(key.buf, repo_name);
	g_svn_url = xstrdup(repo_name);
	strbuf_release(&key);

	remote = remote_get(option_origin);

	/* Initialize APR / libsvn */
	if (svn_cmdline_init("git-isvn", stderr) != EXIT_SUCCESS)
		die("svn_cmdline_init");

	first_client = get_svn_ctx();
	if (first_client == NULL)
		die("Could not connect to SVN server.");

	err = svn_ra_get_latest_revnum(first_client->svn_session, &latest_rev,
		first_client->svn_pool);
	assert_noerr(err, "svn_ra_get_latest_revnum");

	printf("XXX maxrev: %ju\n", (uintmax_t)latest_rev);

	if (option_maxrev == 0 || option_maxrev > latest_rev)
		option_maxrev = latest_rev;

	isvn_fetch(first_client, remote);

	/* Branch 'master' from remote trunk and checkout. */
	checkout();

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

int cmd_isvn(int argc, const char **argv, const char *prefix)
{
	struct option empty[] = { OPT_END() };
	unsigned i;

	/* Skip over 'isvn' */
	argc--;
	argv++;
	if (argc == 0)
		usage_with_options(builtin_isvn_usage, empty);	/* NORETURN */

	/* Dispatch subcommands ... */
	for (i = 0; i < ARRAY_SIZE(isvn_subcommands); i++) {
		if (strcmp(argv[0], isvn_subcommands[i].sub_name) == 0) {
			isvn_subcommands[i].sub_cmd(argc, argv, prefix);
			return 0;
		}
	}

	printf("No such command `%s'\n", argv[0]);
	usage_with_options(builtin_isvn_usage, empty);	/* NORETURN */
}
