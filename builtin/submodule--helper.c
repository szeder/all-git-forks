#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "quote.h"
#include "pathspec.h"
#include "dir.h"
#include "utf8.h"
#include "submodule.h"
#include "submodule-config.h"
#include "string-list.h"
#include "thread-utils.h"
#include "run-command.h"
#ifndef NO_PTHREADS
#include <semaphore.h>
#endif
static const struct cache_entry **ce_entries;
static int ce_alloc, ce_used;
static const char *alternative_path;

static int module_list_compute(int argc, const char **argv,
				const char *prefix,
				struct pathspec *pathspec)
{
	int i;
	char *max_prefix, *ps_matched = NULL;
	int max_prefix_len;
	parse_pathspec(pathspec, 0,
		       PATHSPEC_PREFER_FULL |
		       PATHSPEC_STRIP_SUBMODULE_SLASH_CHEAP,
		       prefix, argv);

	/* Find common prefix for all pathspec's */
	max_prefix = common_prefix(pathspec);
	max_prefix_len = max_prefix ? strlen(max_prefix) : 0;

	if (pathspec->nr)
		ps_matched = xcalloc(pathspec->nr, 1);

	if (read_cache() < 0)
		die("index file corrupt");

	for (i = 0; i < active_nr; i++) {
		const struct cache_entry *ce = active_cache[i];

		if (!match_pathspec(pathspec, ce->name, ce_namelen(ce),
				    max_prefix_len, ps_matched,
				    S_ISGITLINK(ce->ce_mode) | S_ISDIR(ce->ce_mode)))
			continue;

		if (S_ISGITLINK(ce->ce_mode)) {
			ALLOC_GROW(ce_entries, ce_used + 1, ce_alloc);
			ce_entries[ce_used++] = ce;
		}

		while (i + 1 < active_nr && !strcmp(ce->name, active_cache[i + 1]->name))
			/*
			 * Skip entries with the same name in different stages
			 * to make sure an entry is returned only once.
			 */
			i++;
	}
	free(max_prefix);

	if (ps_matched && report_path_error(ps_matched, pathspec, prefix))
		return -1;

	return 0;
}

static int module_list(int argc, const char **argv, const char *prefix)
{
	int i;
	static struct pathspec pathspec;

	struct option module_list_options[] = {
		OPT_STRING(0, "prefix", &alternative_path,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_END()
	};

	static const char * const git_submodule_helper_usage[] = {
		N_("git submodule--helper module_list [--prefix=<path>] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_list_options,
			     git_submodule_helper_usage, 0);

	if (module_list_compute(argc, argv, alternative_path
					    ? alternative_path
					    : prefix, &pathspec) < 0) {
		printf("#unmatched\n");
		return 1;
	}

	for (i = 0; i < ce_used; i++) {
		const struct cache_entry *ce = ce_entries[i];

		if (ce_stage(ce)) {
			printf("%06o %s U\t", ce->ce_mode, sha1_to_hex(null_sha1));
		} else {
			printf("%06o %s %d\t", ce->ce_mode, sha1_to_hex(ce->sha1), ce_stage(ce));
		}

		utf8_fprintf(stdout, "%s\n", ce->name);
	}
	return 0;
}

static int module_name(int argc, const char **argv, const char *prefix)
{
	const char *name;
	const struct submodule *sub;

	if (argc != 1)
		usage("git submodule--helper module_name <path>\n");

	gitmodules_config();
	sub = submodule_from_path(null_sha1, argv[0]);

	if (!sub)
		die("No submodule mapping found in .gitmodules for path '%s'", argv[0]);

	name = sub->name;
	printf("%s\n", name);

	return 0;
}

static int clone_submodule(const char *path, const char *gitdir, const char *url,
			   const char *depth, const char *reference, int quiet)
{
	struct child_process cp;
	child_process_init(&cp);

	argv_array_push(&cp.args, "clone");
	argv_array_push(&cp.args, "--no-checkout");
	if (quiet)
		argv_array_push(&cp.args, "--quiet");
	if (depth && strcmp(depth, "")) {
		argv_array_push(&cp.args, "--depth");
		argv_array_push(&cp.args, depth);
	}
	if (reference && strcmp(reference, "")) {
		argv_array_push(&cp.args, "--reference");
		argv_array_push(&cp.args, reference);
	}
	if (gitdir) {
		argv_array_push(&cp.args, "--separate-git-dir");
		argv_array_push(&cp.args, gitdir);
	}
	argv_array_push(&cp.args, url);
	argv_array_push(&cp.args, path);

	cp.git_cmd = 1;
	cp.env = local_repo_env;

	cp.no_stdin = 1;
	cp.no_stdout = 1;
	cp.no_stderr = 1;

	return run_command(&cp);
}

/*
 * Clone a submodule
 *
 * $1 = submodule path
 * $2 = submodule name
 * $3 = URL to clone
 * $4 = reference repository to reuse (empty for independent)
 * $5 = depth argument for shallow clones (empty for deep)
 *
 * Prior to calling, cmd_update checks that a possibly existing
 * path is not a git repository.
 * Likewise, cmd_add checks that path does not exist at all,
 * since it is the location of a new submodule.
 */
static int module_clone(int argc, const char **argv, const char *prefix)
{
	const char *path = NULL, *name = NULL, *url = NULL, *reference = NULL, *depth = NULL;
	int quiet = 0;
	FILE *submodule_dot_git;
	const char *sm_gitdir, *p;
	struct strbuf rel_path = STRBUF_INIT;
	struct strbuf sb = STRBUF_INIT;

	struct option module_update_options[] = {
		OPT_STRING(0, "prefix", &alternative_path,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_STRING(0, "path", &path,
			   N_("path"),
			   N_("where the new submodule will be cloned to")),
		OPT_STRING(0, "name", &name,
			   N_("string"),
			   N_("name of the new submodule")),
		OPT_STRING(0, "url", &url,
			   N_("string"),
			   N_("url where to clone the submodule from")),
		OPT_STRING(0, "reference", &reference,
			   N_("string"),
			   N_("reference repository")),
		OPT_STRING(0, "depth", &depth,
			   N_("string"),
			   N_("depth for shallow clones")),
		OPT_END()
	};

	static const char * const git_submodule_helper_usage[] = {
		N_("git submodule--helper update [--prefix=<path>] [--quiet] [--remote] [-N|--no-fetch]"
		   "[-f|--force] [--rebase|--merge] [--reference <repository>]"
		   "[--depth <depth>] [--recursive] [--] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_update_options,
			     git_submodule_helper_usage, 0);

	if (getenv("GIT_QUIET"))
		quiet = 1;

	strbuf_addf(&sb, "%s/modules/%s", get_git_dir(), name);
	sm_gitdir = strbuf_detach(&sb, NULL);

	if (!file_exists(sm_gitdir)) {
		safe_create_leading_directories_const(sm_gitdir);
		if (clone_submodule(path, sm_gitdir, url, depth, reference, quiet))
			die(N_("Clone of '%s' into submodule path '%s' failed"),
			    url, path);
	} else {
		safe_create_leading_directories_const(path);
		unlink(sm_gitdir);
	}

	/* Write a .git file in the submodule to redirect to the superproject. */
	if (alternative_path && !strcmp(alternative_path, "")) {
		p = relative_path(path, alternative_path, &sb);
		strbuf_reset(&sb);
	} else
		p = path;

	if (safe_create_leading_directories_const(p) < 0)
		die("Could not create directory '%s'", p);

	strbuf_addf(&sb, "%s/.git", p);

	if (safe_create_leading_directories_const(sb.buf) < 0)
		die(_("could not create leading directories of '%s'"), sb.buf);
	submodule_dot_git = fopen(sb.buf, "w");
	if (!submodule_dot_git)
		die ("Cannot open file '%s': %s", sb.buf, strerror(errno));

	fprintf(submodule_dot_git, "gitdir: %s\n",
		relative_path(sm_gitdir, path, &rel_path));
	if (fclose(submodule_dot_git))
		die("Could not close file %s", sb.buf);
	strbuf_reset(&sb);

	/* Redirect the worktree of the submodule in the superprojects config */
	if (!is_absolute_path(sm_gitdir)) {
		char *s = (char*)sm_gitdir;
		if (strbuf_getcwd(&sb))
			die_errno("unable to get current working directory");
		strbuf_addf(&sb, "/%s", sm_gitdir);
		sm_gitdir = strbuf_detach(&sb, NULL);
		free(s);
	}

	if (strbuf_getcwd(&sb))
		die_errno("unable to get current working directory");
	strbuf_addf(&sb, "/%s", path);

	p = git_pathdup_submodule(path, "config");
	if (!p)
		die("Could not get submodule directory for '%s'", path);
	git_config_set_in_file(p, "core.worktree",
			       relative_path(sb.buf, sm_gitdir, &rel_path));
	strbuf_release(&sb);
	return 0;
}

struct submodule_args {
	const char *name;
	const char *path;
	const char *sha1;
	const char *toplevel;
	const char *prefix;
	const char **cmd;
	struct submodule_output *out;
	sem_t *mutex;
};

int run_cmd_submodule(struct task_queue *aq, void *task)
{
	int i;
	struct submodule_args *args = task;
	struct strbuf out = STRBUF_INIT;
	struct strbuf sb = STRBUF_INIT;
	struct child_process *cp = xmalloc(sizeof(*cp));
	char buf[1024];

	strbuf_addf(&out, N_("Entering %s\n"), relative_path(args->path, args->prefix, &sb));

	child_process_init(cp);
	argv_array_pushv(&cp->args, args->cmd);

	argv_array_pushf(&cp->env_array, "name=%s", args->name);
	argv_array_pushf(&cp->env_array, "path=%s", args->path);
	argv_array_pushf(&cp->env_array, "sha1=%s", args->sha1);
	argv_array_pushf(&cp->env_array, "toplevel=%s", args->toplevel);

	for (i = 0; local_repo_env[i]; i++)
		argv_array_push(&cp->env_array, local_repo_env[i]);

	cp->no_stdin = 1;
	cp->out = 0;
	cp->err = -1;
	cp->dir = args->path;
	cp->stdout_to_stderr = 1;
	cp->use_shell = 1;

	if (start_command(cp)) {
		die("Could not start command");
		for (i = 0; cp->args.argv; i++)
			fprintf(stderr, "%s\n", cp->args.argv[i]);
	}

	while (1) {
		ssize_t len = xread(cp->err, buf, sizeof(buf));
		if (len < 0)
			die("Read from child failed");
		else if (len == 0)
			break;
		else {
			strbuf_add(&out, buf, len);
		}
	}
	if (finish_command(cp))
		die("command died with error");

	sem_wait(args->mutex);
	fputs(out.buf, stderr);
	sem_post(args->mutex);

	return 0;
}

int module_foreach_parallel(int argc, const char **argv, const char *prefix)
{
	int i, recursive = 0, number_threads = 1, quiet = 0;
	static struct pathspec pathspec;
	struct strbuf sb = STRBUF_INIT;
	struct task_queue *aq;
	char **cmd;
	const char **nullargv = {NULL};
	sem_t *mutex = xmalloc(sizeof(*mutex));

	struct option module_update_options[] = {
		OPT_STRING(0, "prefix", &alternative_path,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_STRING(0, "cmd", &cmd,
			   N_("string"),
			   N_("command to run")),
		OPT_BOOL('r', "--recursive", &recursive,
			 N_("Recurse into nexted submodules")),
		OPT_INTEGER('j', "jobs", &number_threads,
			    N_("Recurse into nexted submodules")),
		OPT__QUIET(&quiet, N_("Suppress output")),
		OPT_END()
	};

	static const char * const git_submodule_helper_usage[] = {
		N_("git submodule--helper foreach [--prefix=<path>] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_update_options,
			     git_submodule_helper_usage, 0);

	if (module_list_compute(0, nullargv, NULL, &pathspec) < 0)
		return 1;

	gitmodules_config();

	aq = create_task_queue(number_threads);

	for (i = 0; i < ce_used; i++) {
		const struct submodule *sub;
		const struct cache_entry *ce = ce_entries[i];
		struct submodule_args *args = malloc(sizeof(*args));

		if (ce_stage(ce))
			args->sha1 = xstrdup(sha1_to_hex(null_sha1));
		else
			args->sha1 = xstrdup(sha1_to_hex(ce->sha1));

		strbuf_reset(&sb);
		strbuf_addf(&sb, "%s/.git", ce->name);
		if (!file_exists(sb.buf))
			continue;

		args->path = ce->name;
		sub = submodule_from_path(null_sha1, args->path);
		if (!sub)
			die("No submodule mapping found in .gitmodules for path '%s'", args->path);

		args->name = sub->name;
		args->toplevel = xstrdup(xgetcwd());
		args->cmd = argv;
		args->mutex = mutex;
		args->prefix = alternative_path;
		add_task(aq, run_cmd_submodule, args);
	}

	finish_task_queue(aq, NULL);
	return 0;
}

int cmd_submodule__helper(int argc, const char **argv, const char *prefix)
{
	if (argc < 2)
		goto usage;

	if (!strcmp(argv[1], "module_list"))
		return module_list(argc - 1, argv + 1, prefix);

	if (!strcmp(argv[1], "module_name"))
		return module_name(argc - 2, argv + 2, prefix);

	if (!strcmp(argv[1], "module_clone"))
		return module_clone(argc - 1, argv + 1, prefix);

	if (!strcmp(argv[1], "foreach"))
		return module_foreach_parallel(argc - 1, argv + 1, prefix);

usage:
	usage("git submodule--helper [module_list module_name module_clone foreach]\n");
}
