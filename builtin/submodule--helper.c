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
#include "run-command.h"

struct dotmodule_list {
    char **entries;
    int alloc, nr;
};

struct module_list {
	const struct cache_entry **entries;
	int alloc, nr;
};
#define MODULE_LIST_INIT { NULL, 0, 0 }

static int is_gitdir_submodule(const char *path)
{
	struct strbuf buf = STRBUF_INIT;
	int is_submodule;

	strbuf_addf(&buf, "%s/index", path);
	is_submodule = file_exists(buf.buf);

	strbuf_release(&buf);

	return is_submodule;
}

static int module_list_gitdir_modules(const char *current_dir,
				      struct dotmodule_list *list,
				      struct pathspec *pathspec,
				      char *ps_matched,
				      int max_prefix_len)
{
	struct strbuf path = STRBUF_INIT;
	struct dirent *entry;
	DIR *dir = NULL;
	int ret = 0;

	strbuf_git_path(&path, "modules");

	if (current_dir) {
		strbuf_addf(&path, "/%s", current_dir);

		if (is_gitdir_submodule(path.buf) &&
		    match_pathspec(pathspec, current_dir, strlen(current_dir),
				   max_prefix_len, ps_matched, 1)) {
			ALLOC_GROW(list->entries, list->nr + 1, list->alloc);
			list->entries[list->nr++] = xstrdup(current_dir);
			goto out;
		}
	}

	if ((dir = opendir(path.buf)) == NULL)
		goto out;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type != DT_DIR)
			continue;
		if (!strcmp(entry->d_name, ".") ||
		    !strcmp(entry->d_name, ".."))
			continue;

		strbuf_reset(&path);
		if (current_dir)
			strbuf_addf(&path, "%s/", current_dir);
		strbuf_addstr(&path, entry->d_name);

		module_list_gitdir_modules(path.buf, list, pathspec, ps_matched, max_prefix_len);
	}

out:
	closedir(dir);
	strbuf_release(&path);

	return ret;
}

static int cmp_gitdir_modules(const void *m1, const void *m2)
{
	return strcmp(*(char * const *)m1, *(char * const *)m2);
}

static int module_list_compute_all(int argc, const char **argv,
				   const char *prefix,
				   struct pathspec *pathspec)
{
	struct dotmodule_list list = MODULE_LIST_INIT;
	char *max_prefix, *ps_matched = NULL;
	int max_prefix_len;
	int i;

	parse_pathspec(pathspec, 0,
		       PATHSPEC_PREFER_FULL |
		       PATHSPEC_STRIP_SUBMODULE_SLASH_CHEAP,
		       prefix, argv);

	max_prefix = common_prefix(pathspec);
	max_prefix_len = max_prefix ? strlen(max_prefix) : 0;

	if (pathspec->nr)
		ps_matched = xcalloc(pathspec->nr, 1);

	module_list_gitdir_modules(NULL, &list, pathspec, ps_matched, max_prefix_len);
	qsort(&list.entries[0], list.nr, sizeof(char *), cmp_gitdir_modules);

	for (i = 0; i < list.nr; i++) {
		puts(list.entries[i]);
	}

	if (ps_matched && report_path_error(ps_matched, pathspec, prefix))
		return -1;

	free(ps_matched);

	return 0;
}

static int module_list_compute_index(int argc, const char **argv,
				     const char *prefix,
				     struct pathspec *pathspec,
				     struct module_list *list)
{
	int i, result = 0;
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
		die(_("index file corrupt"));

	for (i = 0; i < active_nr; i++) {
		const struct cache_entry *ce = active_cache[i];

		if (!S_ISGITLINK(ce->ce_mode) ||
		    !match_pathspec(pathspec, ce->name, ce_namelen(ce),
				    max_prefix_len, ps_matched, 1))
			continue;

		ALLOC_GROW(list->entries, list->nr + 1, list->alloc);
		list->entries[list->nr++] = ce;
		while (i + 1 < active_nr &&
		       !strcmp(ce->name, active_cache[i + 1]->name))
			/*
			 * Skip entries with the same name in different stages
			 * to make sure an entry is returned only once.
			 */
			i++;
	}
	free(max_prefix);

	if (ps_matched && report_path_error(ps_matched, pathspec, prefix))
		result = -1;

	free(ps_matched);

	return result;
}

static int module_list(int argc, const char **argv, const char *prefix)
{
	int i;
	int all = 0;
	struct pathspec pathspec;
	struct module_list list = MODULE_LIST_INIT;

	struct option module_list_options[] = {
		OPT_STRING(0, "prefix", &prefix,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_BOOL(0, "all", &all,
			   N_("also include submodules not currently in .gitmodules")),
		OPT_END()
	};

	const char *const git_submodule_helper_usage[] = {
		N_("git submodule--helper list [--prefix=<path>] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_list_options,
			     git_submodule_helper_usage, 0);

	if (all) {
	    module_list_compute_all(argc, argv, prefix, &pathspec);
	} else {
	    if (module_list_compute_index(argc, argv, prefix, &pathspec, &list) < 0) {
			printf("#unmatched\n");
			return 1;
	    }
	}

	for (i = 0; i < list.nr; i++) {
		const struct cache_entry *ce = list.entries[i];

		if (ce_stage(ce))
			printf("%06o %s U\t", ce->ce_mode, sha1_to_hex(null_sha1));
		else
			printf("%06o %s %d\t", ce->ce_mode, sha1_to_hex(ce->sha1), ce_stage(ce));

		utf8_fprintf(stdout, "%s\n", ce->name);
	}
	return 0;
}

static int module_name(int argc, const char **argv, const char *prefix)
{
	const struct submodule *sub;

	if (argc != 2)
		usage(_("git submodule--helper name <path>"));

	gitmodules_config();
	sub = submodule_from_path(null_sha1, argv[1]);

	if (!sub)
		die(_("no submodule mapping found in .gitmodules for path '%s'"),
		    argv[1]);

	printf("%s\n", sub->name);

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
	if (depth && *depth)
		argv_array_pushl(&cp.args, "--depth", depth, NULL);
	if (reference && *reference)
		argv_array_pushl(&cp.args, "--reference", reference, NULL);
	if (gitdir && *gitdir)
		argv_array_pushl(&cp.args, "--separate-git-dir", gitdir, NULL);

	argv_array_push(&cp.args, url);
	argv_array_push(&cp.args, path);

	cp.git_cmd = 1;
	cp.env = local_repo_env;
	cp.no_stdin = 1;

	return run_command(&cp);
}

static int module_clone(int argc, const char **argv, const char *prefix)
{
	const char *path = NULL, *name = NULL, *url = NULL;
	const char *reference = NULL, *depth = NULL;
	int quiet = 0;
	FILE *submodule_dot_git;
	char *sm_gitdir, *cwd, *p;
	struct strbuf rel_path = STRBUF_INIT;
	struct strbuf sb = STRBUF_INIT;

	struct option module_clone_options[] = {
		OPT_STRING(0, "prefix", &prefix,
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
		OPT__QUIET(&quiet, "Suppress output for cloning a submodule"),
		OPT_END()
	};

	const char *const git_submodule_helper_usage[] = {
		N_("git submodule--helper clone [--prefix=<path>] [--quiet] "
		   "[--reference <repository>] [--name <name>] [--url <url>]"
		   "[--depth <depth>] [--] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_clone_options,
			     git_submodule_helper_usage, 0);

	strbuf_addf(&sb, "%s/modules/%s", get_git_dir(), name);
	sm_gitdir = strbuf_detach(&sb, NULL);

	if (!file_exists(sm_gitdir)) {
		if (safe_create_leading_directories_const(sm_gitdir) < 0)
			die(_("could not create directory '%s'"), sm_gitdir);
		if (clone_submodule(path, sm_gitdir, url, depth, reference, quiet))
			die(_("clone of '%s' into submodule path '%s' failed"),
			    url, path);
	} else {
		if (safe_create_leading_directories_const(path) < 0)
			die(_("could not create directory '%s'"), path);
		strbuf_addf(&sb, "%s/index", sm_gitdir);
		unlink_or_warn(sb.buf);
		strbuf_reset(&sb);
	}

	/* Write a .git file in the submodule to redirect to the superproject. */
	if (safe_create_leading_directories_const(path) < 0)
		die(_("could not create directory '%s'"), path);

	if (path && *path)
		strbuf_addf(&sb, "%s/.git", path);
	else
		strbuf_addstr(&sb, ".git");

	if (safe_create_leading_directories_const(sb.buf) < 0)
		die(_("could not create leading directories of '%s'"), sb.buf);
	submodule_dot_git = fopen(sb.buf, "w");
	if (!submodule_dot_git)
		die_errno(_("cannot open file '%s'"), sb.buf);

	fprintf(submodule_dot_git, "gitdir: %s\n",
		relative_path(sm_gitdir, path, &rel_path));
	if (fclose(submodule_dot_git))
		die(_("could not close file %s"), sb.buf);
	strbuf_reset(&sb);
	strbuf_reset(&rel_path);

	cwd = xgetcwd();
	/* Redirect the worktree of the submodule in the superproject's config */
	if (!is_absolute_path(sm_gitdir)) {
		strbuf_addf(&sb, "%s/%s", cwd, sm_gitdir);
		free(sm_gitdir);
		sm_gitdir = strbuf_detach(&sb, NULL);
	}

	strbuf_addf(&sb, "%s/%s", cwd, path);
	p = git_pathdup_submodule(path, "config");
	if (!p)
		die(_("could not get submodule directory for '%s'"), path);
	git_config_set_in_file(p, "core.worktree",
			       relative_path(sb.buf, sm_gitdir, &rel_path));
	strbuf_release(&sb);
	strbuf_release(&rel_path);
	free(sm_gitdir);
	free(cwd);
	free(p);
	return 0;
}

struct cmd_struct {
	const char *cmd;
	int (*fn)(int, const char **, const char *);
};

static struct cmd_struct commands[] = {
	{"list", module_list},
	{"name", module_name},
	{"clone", module_clone},
};

int cmd_submodule__helper(int argc, const char **argv, const char *prefix)
{
	int i;
	if (argc < 2)
		die(_("fatal: submodule--helper subcommand must be "
		      "called with a subcommand"));

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		if (!strcmp(argv[1], commands[i].cmd))
			return commands[i].fn(argc - 1, argv + 1, prefix);

	die(_("fatal: '%s' is not a valid submodule--helper "
	      "subcommand"), argv[1]);
}
