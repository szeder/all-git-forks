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
#include "remote.h"
#include "refs.h"
#include "connect.h"

static char *get_default_remote(void)
{
	char *dest = NULL, *ret;
	unsigned char sha1[20];
	int flag = 0;
	struct strbuf sb = STRBUF_INIT;
	const char *refname = resolve_ref_unsafe("HEAD", 0, sha1, &flag);

	if (!refname)
		die("No such ref: HEAD");

	/* detached HEAD */
	if (!strcmp(refname, "HEAD"))
		return xstrdup("origin");

	if (!skip_prefix(refname, "refs/heads/", &refname))
		die(_("Expecting a full ref name, got %s"), refname);

	strbuf_addf(&sb, "branch.%s.remote", refname);
	if (git_config_get_string(sb.buf, &dest))
		ret = xstrdup("origin");
	else
		ret = xstrdup(dest);

	strbuf_release(&sb);
	return ret;
}

static int starts_with_dot_slash(const char *str)
{
	return str[0] == '.' && is_dir_sep(str[1]);
}

static int starts_with_dot_dot_slash(const char *str)
{
	return str[0] == '.' && str[1] == '.' && is_dir_sep(str[2]);
}

/*
 * Returns 1 if it was the last chop before ':'.
 */
static int chop_last_dir(char **remoteurl, int is_relative)
{
	char *rfind = find_last_dir_sep(*remoteurl);
	if (rfind) {
		*rfind = '\0';
		return 0;
	}

	rfind = strrchr(*remoteurl, ':');
	if (rfind) {
		*rfind = '\0';
		return 1;
	}

	if (is_relative || !strcmp(".", *remoteurl))
		die(_("cannot strip one component off url '%s'"),
			*remoteurl);

	free(*remoteurl);
	*remoteurl = xstrdup(".");
	return 0;
}

/*
 * The `url` argument is the URL that navigates to the submodule origin
 * repo. When relative, this URL is relative to the superproject origin
 * URL repo. The `up_path` argument, if specified, is the relative
 * path that navigates from the submodule working tree to the superproject
 * working tree. Returns the origin URL of the submodule.
 *
 * Return either an absolute URL or filesystem path (if the superproject
 * origin URL is an absolute URL or filesystem path, respectively) or a
 * relative file system path (if the superproject origin URL is a relative
 * file system path).
 *
 * When the output is a relative file system path, the path is either
 * relative to the submodule working tree, if up_path is specified, or to
 * the superproject working tree otherwise.
 *
 * NEEDSWORK: This works incorrectly on the domain and protocol part.
 * remote_url      url              outcome          correct
 * http://a.com/b  ../c             http://a.com/c   yes
 * http://a.com/b  ../../c          http://c         no (domain should be kept)
 * http://a.com/b  ../../../c       http:/c          no
 * http://a.com/b  ../../../../c    http:c           no
 * http://a.com/b  ../../../../../c    .:c           no
 */
static char *relative_url(const char *remote_url,
				const char *url,
				const char *up_path)
{
	int is_relative = 0;
	int colonsep = 0;
	char *out;
	char *remoteurl = xstrdup(remote_url);
	struct strbuf sb = STRBUF_INIT;
	size_t len = strlen(remoteurl);

	if (is_dir_sep(remoteurl[len]))
		remoteurl[len] = '\0';

	if (!url_is_local_not_ssh(remoteurl) || is_absolute_path(remoteurl))
		is_relative = 0;
	else {
		is_relative = 1;
		/*
		 * Prepend a './' to ensure all relative
		 * remoteurls start with './' or '../'
		 */
		if (!starts_with_dot_slash(remoteurl) &&
		    !starts_with_dot_dot_slash(remoteurl)) {
			strbuf_reset(&sb);
			strbuf_addf(&sb, "./%s", remoteurl);
			free(remoteurl);
			remoteurl = strbuf_detach(&sb, NULL);
		}
	}
	/*
	 * When the url starts with '../', remove that and the
	 * last directory in remoteurl.
	 */
	while (url) {
		if (starts_with_dot_dot_slash(url)) {
			url += 3;
			colonsep |= chop_last_dir(&remoteurl, is_relative);
		} else if (starts_with_dot_slash(url))
			url += 2;
		else
			break;
	}
	strbuf_reset(&sb);
	strbuf_addf(&sb, "%s%s%s", remoteurl, colonsep ? ":" : "/", url);

	if (starts_with_dot_slash(sb.buf))
		out = xstrdup(sb.buf + 2);
	else
		out = xstrdup(sb.buf);
	strbuf_reset(&sb);

	free(remoteurl);
	if (!up_path || !is_relative)
		return out;

	strbuf_addf(&sb, "%s%s", up_path, out);
	free(out);
	return strbuf_detach(&sb, NULL);
}

static int resolve_relative_url(int argc, const char **argv, const char *prefix)
{
	char *remoteurl = NULL;
	char *remote = get_default_remote();
	const char *up_path = NULL;
	char *res;
	const char *url;
	struct strbuf sb = STRBUF_INIT;

	if (argc != 2 && argc != 3)
		die("resolve-relative-url only accepts one or two arguments");

	url = argv[1];
	strbuf_addf(&sb, "remote.%s.url", remote);
	free(remote);

	if (git_config_get_string(sb.buf, &remoteurl))
		/* the repository is its own authoritative upstream */
		remoteurl = xgetcwd();

	if (argc == 3)
		up_path = argv[2];

	res = relative_url(remoteurl, url, up_path);
	puts(res);
	free(res);
	return 0;
}

static int resolve_relative_url_test(int argc, const char **argv, const char *prefix)
{
	char *remoteurl, *res;
	const char *up_path, *url;

	if (argc != 4)
		die("resolve-relative-url-test only accepts three arguments: <up_path> <remoteurl> <url>");

	up_path = argv[1];
	remoteurl = xstrdup(argv[2]);
	url = argv[3];

	if (!strcmp(up_path, "(null)"))
		up_path = NULL;

	res = relative_url(remoteurl, url, up_path);
	puts(res);
	free(res);
	return 0;
}

static int git_submodule_config(const char *var, const char *value, void *cb)
{
	return parse_submodule_config_option(var, value);
}

static void init_submodule(const char *path, const char *prefix, int quiet)
{
	const struct submodule *sub;
	struct strbuf sb = STRBUF_INIT;
	char *url = NULL;
	const char *upd = NULL;
	const char *displaypath = relative_path(xgetcwd(), prefix, &sb);

	/* Only loads from .gitmodules, no overlay with .git/config */
	gitmodules_config();

	sub = submodule_from_path(null_sha1, path);

	/*
	 * Copy url setting when it is not set yet.
	 * To look up the url in .git/config, we must not fall back to
	 * .gitmodules, so look it up directly.
	 */
	strbuf_reset(&sb);
	strbuf_addf(&sb, "submodule.%s.url", sub->name);
	if (git_config_get_string(sb.buf, &url)) {
		url = xstrdup(sub->url);

		if (!url)
			die(_("No url found for submodule path '%s' in .gitmodules"),
				displaypath);

		/* Possibly a url relative to parent */
		if (starts_with_dot_dot_slash(url) ||
		    starts_with_dot_slash(url)) {
			char *remoteurl;
			char *remote = get_default_remote();
			struct strbuf remotesb = STRBUF_INIT;
			strbuf_addf(&remotesb, "remote.%s.url", remote);
			free(remote);

			if (git_config_get_string(remotesb.buf, &remoteurl))
				/*
				 * The repository is its own
				 * authoritative upstream
				 */
				remoteurl = xgetcwd();
			url = relative_url(remoteurl, url, NULL);
			strbuf_release(&remotesb);
		}

		if (git_config_set(sb.buf, url))
			die(_("Failed to register url for submodule path '%s'"),
			    displaypath);
		if (!quiet)
			fprintf(stderr, _("Submodule '%s' (%s) registered for path '%s'\n"),
				sub->name, url, displaypath);
		free(url);
	}

	/* Copy "update" setting when it is not set yet */
	strbuf_reset(&sb);
	strbuf_addf(&sb, "submodule.%s.update", sub->name);
	if (git_config_get_string_const(sb.buf, &upd) && sub->update) {
		upd = sub->update;
		if (strcmp(sub->update, "checkout") &&
		    strcmp(sub->update, "rebase") &&
		    strcmp(sub->update, "merge") &&
		    strcmp(sub->update, "none")) {
			fprintf(stderr, _("warning: unknown update mode '%s' suggested for submodule '%s'\n"),
				upd, sub->name);
			upd = "none";
		}
		if (git_config_set(sb.buf, upd))
			die(_("Failed to register update mode for submodule path '%s'"), displaypath);
	}
	strbuf_release(&sb);
}

static int module_init(int argc, const char **argv, const char *prefix)
{
	int quiet = 0;
	int i;

	struct option module_init_options[] = {
		OPT_STRING(0, "prefix", &prefix,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT__QUIET(&quiet, "Suppress output for initialzing a submodule"),
		OPT_END()
	};

	const char *const git_submodule_helper_usage[] = {
		N_("git submodule--helper init [<path>]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_init_options,
			     git_submodule_helper_usage, 0);

	if (argc == 0)
		die(_("Pass at least one submodule"));

	for (i = 0; i < argc; i++)
		init_submodule(argv[i], prefix, quiet);

	return 0;
}

struct module_list {
	const struct cache_entry **entries;
	int alloc, nr;
};
#define MODULE_LIST_INIT { NULL, 0, 0 }

static int module_list_compute(int argc, const char **argv,
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
	struct pathspec pathspec;
	struct module_list list = MODULE_LIST_INIT;

	struct option module_list_options[] = {
		OPT_STRING(0, "prefix", &prefix,
			   N_("path"),
			   N_("alternative anchor for relative paths")),
		OPT_END()
	};

	const char *const git_submodule_helper_usage[] = {
		N_("git submodule--helper list [--prefix=<path>] [<path>...]"),
		NULL
	};

	argc = parse_options(argc, argv, prefix, module_list_options,
			     git_submodule_helper_usage, 0);

	if (module_list_compute(argc, argv, prefix, &pathspec, &list) < 0) {
		printf("#unmatched\n");
		return 1;
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

struct submodule_update_clone {
	/* states */
	int count;
	int print_unmatched;
	/* configuration */
	int quiet;
	const char *reference;
	const char *depth;
	const char *update;
	const char *recursive_prefix;
	const char *prefix;
	struct module_list list;
	struct string_list projectlines;
	struct pathspec pathspec;
};
#define SUBMODULE_UPDATE_CLONE_INIT {0, 0, 0, NULL, NULL, NULL, NULL, NULL, MODULE_LIST_INIT, STRING_LIST_INIT_DUP}

static void fill_clone_command(struct child_process *cp, int quiet,
			       const char *prefix, const char *path,
			       const char *name, const char *url,
			       const char *reference, const char *depth)
{
	cp->git_cmd = 1;
	cp->no_stdin = 1;
	cp->stdout_to_stderr = 1;
	cp->err = -1;
	argv_array_push(&cp->args, "submodule--helper");
	argv_array_push(&cp->args, "clone");
	if (quiet)
		argv_array_push(&cp->args, "--quiet");

	if (prefix)
		argv_array_pushl(&cp->args, "--prefix", prefix, NULL);

	argv_array_pushl(&cp->args, "--path", path, NULL);
	argv_array_pushl(&cp->args, "--name", name, NULL);
	argv_array_pushl(&cp->args, "--url", url, NULL);
	if (reference)
		argv_array_push(&cp->args, reference);
	if (depth)
		argv_array_push(&cp->args, depth);
}

static int update_clone_get_next_task(struct child_process *cp,
				      struct strbuf *err,
				      void *pp_cb,
				      void **pp_task_cb)
{
	struct submodule_update_clone *pp = pp_cb;

	for (; pp->count < pp->list.nr; pp->count++) {
		const struct submodule *sub = NULL;
		const char *displaypath = NULL;
		const struct cache_entry *ce = pp->list.entries[pp->count];
		struct strbuf sb = STRBUF_INIT;
		const char *update_module = NULL;
		char *url = NULL;
		int needs_cloning = 0;

		if (ce_stage(ce)) {
			if (pp->recursive_prefix)
				strbuf_addf(err, "Skipping unmerged submodule %s/%s\n",
					pp->recursive_prefix, ce->name);
			else
				strbuf_addf(err, "Skipping unmerged submodule %s\n",
					ce->name);
			continue;
		}

		sub = submodule_from_path(null_sha1, ce->name);
		if (!sub) {
			strbuf_addf(err, "BUG: internal error managing submodules. "
				    "The cache could not locate '%s'", ce->name);
			pp->print_unmatched = 1;
			continue;
		}

		if (pp->recursive_prefix)
			displaypath = relative_path(pp->recursive_prefix, ce->name, &sb);
		else
			displaypath = ce->name;

		if (pp->update)
			update_module = pp->update;
		if (!update_module)
			update_module = sub->update;
		if (!update_module)
			update_module = "checkout";
		if (!strcmp(update_module, "none")) {
			strbuf_addf(err, "Skipping submodule '%s'\n", displaypath);
			continue;
		}

		/*
		 * Looking up the url in .git/config.
		 * We must not fall back to .gitmodules as we only want to process
		 * configured submodules.
		 */
		strbuf_reset(&sb);
		strbuf_addf(&sb, "submodule.%s.url", sub->name);
		git_config_get_string(sb.buf, &url);
		if (!url) {
			/*
			 * Only mention uninitialized submodules when its
			 * path have been specified
			 */
			if (pp->pathspec.nr)
				strbuf_addf(err, _("Submodule path '%s' not initialized\n"
					"Maybe you want to use 'update --init'?"), displaypath);
			continue;
		}

		strbuf_reset(&sb);
		strbuf_addf(&sb, "%s/.git", ce->name);
		needs_cloning = !file_exists(sb.buf);

		strbuf_reset(&sb);
		strbuf_addf(&sb, "%06o %s %d %d\t%s\n", ce->ce_mode,
				sha1_to_hex(ce->sha1), ce_stage(ce),
				needs_cloning, ce->name);
		string_list_append(&pp->projectlines, sb.buf);

		if (needs_cloning) {
			fill_clone_command(cp, pp->quiet, pp->prefix, ce->name,
					   sub->name, url, pp->reference, pp->depth);
			pp->count++;
			free(url);
			return 1;
		} else
			free(url);
	}
	return 0;
}

static int update_clone_start_failure(struct child_process *cp,
				      struct strbuf *err,
				      void *pp_cb,
				      void *pp_task_cb)
{
	struct submodule_update_clone *pp = pp_cb;

	strbuf_addf(err, "error when starting a child process");
	pp->print_unmatched = 1;

	return 1;
}

static int update_clone_task_finished(int result,
				      struct child_process *cp,
				      struct strbuf *err,
				      void *pp_cb,
				      void *pp_task_cb)
{
	struct submodule_update_clone *pp = pp_cb;

	if (!result) {
		return 0;
	} else {
		strbuf_addf(err, "error in one child process");
		pp->print_unmatched = 1;
		return 1;
	}
}

static int update_clone(int argc, const char **argv, const char *prefix)
{
	int max_jobs = -1;
	struct string_list_item *item;
	struct submodule_update_clone pp = SUBMODULE_UPDATE_CLONE_INIT;

	struct option module_list_options[] = {
		OPT_STRING(0, "prefix", &prefix,
			   N_("path"),
			   N_("path into the working tree")),
		OPT_STRING(0, "recursive_prefix", &pp.recursive_prefix,
			   N_("path"),
			   N_("path into the working tree, across nested "
			      "submodule boundaries")),
		OPT_STRING(0, "update", &pp.update,
			   N_("string"),
			   N_("update command for submodules")),
		OPT_STRING(0, "reference", &pp.reference, "<repository>",
			   N_("Use the local reference repository "
			      "instead of a full clone")),
		OPT_STRING(0, "depth", &pp.depth, "<depth>",
			   N_("Create a shallow clone truncated to the "
			      "specified number of revisions")),
		OPT_INTEGER('j', "jobs", &max_jobs,
			    N_("parallel jobs")),
		OPT__QUIET(&pp.quiet, N_("do't print cloning progress")),
		OPT_END()
	};

	const char *const git_submodule_helper_usage[] = {
		N_("git submodule--helper list [--prefix=<path>] [<path>...]"),
		NULL
	};
	pp.prefix = prefix;

	argc = parse_options(argc, argv, prefix, module_list_options,
			     git_submodule_helper_usage, 0);

	if (module_list_compute(argc, argv, prefix, &pp.pathspec, &pp.list) < 0) {
		printf("#unmatched\n");
		return 1;
	}

	gitmodules_config();
	/* Overlay the parsed .gitmodules file with .git/config */
	git_config(git_submodule_config, NULL);

	if (max_jobs < 0)
		max_jobs = config_parallel_submodules();
	if (max_jobs < 0)
		max_jobs = 1;

	run_processes_parallel(max_jobs,
			       update_clone_get_next_task,
			       update_clone_start_failure,
			       update_clone_task_finished,
			       &pp);

	if (pp.print_unmatched) {
		printf("#unmatched\n");
		return 1;
	}

	for_each_string_list_item(item, &pp.projectlines)
		utf8_fprintf(stdout, "%s", item->string);

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
	{"update-clone", update_clone},
	{"resolve-relative-url", resolve_relative_url},
	{"resolve-relative-url-test", resolve_relative_url_test},
	{"init", module_init}
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
