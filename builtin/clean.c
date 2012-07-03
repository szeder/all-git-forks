/*
 * "git clean" builtin command
 *
 * Copyright (C) 2007 Shawn Bohrer
 *
 * Based on git-clean.sh by Pavel Roskin
 */

#include "builtin.h"
#include "cache.h"
#include "dir.h"
#include "parse-options.h"
#include "string-list.h"
#include "tree-walk.h"
#include "unpack-trees.h"
#include "cache-tree.h"
#include "refs.h"
#include "quote.h"

static int force = -1; /* unset */
static int quiet, backup;

static const char *const builtin_clean_usage[] = {
	"git clean [-d] [-f] [-n] [-q] [-e <pattern>] [-x | -X] [--] <paths>...",
	NULL
};

static int git_clean_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "clean.requireforce"))
		force = !git_config_bool(var, value);
	else if (!strcmp(var, "clean.backup"))
		backup = !git_config_bool(var, value);
	return git_default_config(var, value, cb);
}

static int exclude_cb(const struct option *opt, const char *arg, int unset)
{
	struct string_list *exclude_list = opt->value;
	string_list_append(exclude_list, arg);
	return 0;
}

static int backed_up_anything;

static void backup_file(const char *path, struct stat *st)
{
	if (add_to_cache(path, st, 0))
		die(_("updating files failed"));
	backed_up_anything = 1;
}

static void backup_dir_recursively(struct strbuf *path, const char *prefix)
{
	DIR *dir;
	struct dirent *e;
	int original_len = path->len;

	dir = opendir(path->buf);
	if (!dir)
		die_errno(_("opendir('%s') failed"), path->buf);

	while ((e = readdir(dir)) != NULL) {
		struct stat st;
		if (is_dot_or_dotdot(e->d_name))
			continue;

		strbuf_addstr(path, e->d_name);
		if (lstat(path->buf, &st))
			; /* fall through */
		else if (S_ISDIR(st.st_mode)) {
			strbuf_addch(path, '/');
			backup_dir_recursively(path, prefix);
		} else if (S_ISREG(st.st_mode))
			backup_file(path->buf, &st);
		strbuf_setlen(path, original_len);
	}

	closedir(dir);
}

static struct commit_list *parents;

static void prepare_backup(void)
{
	struct unpack_trees_options opts;
	unsigned char sha1[20];
	struct tree *tree;
	struct commit *parent;
	struct tree_desc t;

	if (get_sha1("HEAD", sha1))
		die(_("You do not have the initial commit yet"));

	/* prepare parent-list */
	parent = lookup_commit_or_die(sha1, "HEAD");
	commit_list_insert(parent, &parents);

	/* load HEAD into the index */

	tree = parse_tree_indirect(sha1);
	if (!tree)
		die(_("Failed to unpack tree object %s"), sha1);

	parse_tree(tree);
	init_tree_desc(&t, tree->buffer, tree->size);

	memset(&opts, 0, sizeof(opts));
	opts.head_idx = -1;
	opts.src_index = &the_index;
	opts.dst_index = &the_index;
	opts.index_only = 1;

	if (unpack_trees(1, &t, &opts)) {
		/* We've already reported the error, finish dying */
		exit(128);
	}
}

static void finish_backup(void)
{
	const char *ref = "refs/clean-backup";
	unsigned char commit_sha1[20];
	struct strbuf msg = STRBUF_INIT;
	char logfile[PATH_MAX];
	struct stat st;

	if (!backed_up_anything)
		return;

	if (!active_cache_tree)
		active_cache_tree = cache_tree();

	if (!cache_tree_fully_valid(active_cache_tree)) {
		if (cache_tree_update(active_cache_tree,
		    active_cache, active_nr, 0) < 0)
			die("failed to update cache");
	}

	strbuf_addstr(&msg, "Automatically committed by git-clean");

	/* create a reflog, if there isn't one */
	git_snpath(logfile, sizeof(logfile), "logs/%s", ref);
	if (stat(logfile, &st)) {
		FILE *fp = fopen(logfile, "w");
		if (!fp)
			warning(_("Can not do reflog for '%s'\n"), ref);
		else
			fclose(fp);
	}

	if (commit_tree(&msg, active_cache_tree->sha1, parents, commit_sha1,
	    NULL, NULL))
		die("failed to commit :(");

	update_ref(msg.buf, ref, commit_sha1, NULL, 0, DIE_ON_ERR);
}

int cmd_clean(int argc, const char **argv, const char *prefix)
{
	int i;
	int show_only = 0, remove_directories = 0, ignored = 0;
	int ignored_only = 0, config_set = 0, errors = 0;
	int rm_flags = REMOVE_DIR_KEEP_NESTED_GIT;
	struct strbuf directory = STRBUF_INIT;
	struct dir_struct dir;
	static const char **pathspec;
	struct strbuf buf = STRBUF_INIT;
	struct string_list exclude_list = STRING_LIST_INIT_NODUP;
	const char *qname;
	char *seen = NULL;
	struct option options[] = {
		OPT__QUIET(&quiet, "do not print names of files removed"),
		OPT__DRY_RUN(&show_only, "dry run"),
		OPT__FORCE(&force, "force"),
		OPT_BOOLEAN('d', NULL, &remove_directories,
				"remove whole directories"),
		OPT_BOOLEAN('b', "backup", &backup, "store blobs in "
		  "object-database before deleting them"),
		{ OPTION_CALLBACK, 'e', "exclude", &exclude_list, "pattern",
		  "add <pattern> to ignore rules", PARSE_OPT_NONEG, exclude_cb },
		OPT_BOOLEAN('x', NULL, &ignored, "remove ignored files, too"),
		OPT_BOOLEAN('X', NULL, &ignored_only,
				"remove only ignored files"),
		OPT_END()
	};

	git_config(git_clean_config, NULL);
	if (force < 0)
		force = 0;
	else
		config_set = 1;

	argc = parse_options(argc, argv, prefix, options, builtin_clean_usage,
			     0);

	memset(&dir, 0, sizeof(dir));
	if (ignored_only)
		dir.flags |= DIR_SHOW_IGNORED;

	if (ignored && ignored_only)
		die(_("-x and -X cannot be used together"));

	if (!show_only && !force) {
		if (config_set)
			die(_("clean.requireForce set to true and neither -n nor -f given; "
				  "refusing to clean"));
		else
			die(_("clean.requireForce defaults to true and neither -n nor -f given; "
				  "refusing to clean"));
	}

	if (force > 1)
		rm_flags = 0;

	dir.flags |= DIR_SHOW_OTHER_DIRECTORIES;

	if (read_cache() < 0)
		die(_("index file corrupt"));

	if (!ignored)
		setup_standard_excludes(&dir);

	if (backup && !show_only)
		prepare_backup();

	for (i = 0; i < exclude_list.nr; i++)
		add_exclude(exclude_list.items[i].string, "", 0,
			    &dir.exclude_list[EXC_CMDL]);

	pathspec = get_pathspec(prefix, argv);

	fill_directory(&dir, pathspec);

	if (pathspec)
		seen = xmalloc(argc > 0 ? argc : 1);

	for (i = 0; i < dir.nr; i++) {
		struct dir_entry *ent = dir.entries[i];
		int len, pos;
		int matches = 0;
		struct cache_entry *ce;
		struct stat st;

		/*
		 * Remove the '/' at the end that directory
		 * walking adds for directory entries.
		 */
		len = ent->len;
		if (len && ent->name[len-1] == '/')
			len--;
		pos = cache_name_pos(ent->name, len);
		if (0 <= pos)
			continue;	/* exact match */
		pos = -pos - 1;
		if (pos < active_nr) {
			ce = active_cache[pos];
			if (ce_namelen(ce) == len &&
			    !memcmp(ce->name, ent->name, len))
				continue; /* Yup, this one exists unmerged */
		}

		/*
		 * we might have removed this as part of earlier
		 * recursive directory removal, so lstat() here could
		 * fail with ENOENT.
		 */
		if (lstat(ent->name, &st))
			continue;

		if (pathspec) {
			memset(seen, 0, argc > 0 ? argc : 1);
			matches = match_pathspec(pathspec, ent->name, len,
						 0, seen);
		}

		if (S_ISDIR(st.st_mode)) {
			strbuf_addstr(&directory, ent->name);
			qname = quote_path_relative(directory.buf, directory.len, &buf, prefix);
			if (show_only && (remove_directories ||
			    (matches == MATCHED_EXACTLY))) {
				printf(_("Would remove %s\n"), qname);
			} else if (remove_directories ||
				   (matches == MATCHED_EXACTLY)) {
				if (backup)
					backup_dir_recursively(&directory,
					    prefix);
				if (!quiet)
					printf(_("Removing %s\n"), qname);
				if (remove_dir_recursively(&directory,
							   rm_flags) != 0) {
					warning(_("failed to remove %s"), qname);
					errors++;
				}
			} else if (show_only) {
				printf(_("Would not remove %s\n"), qname);
			} else {
				printf(_("Not removing %s\n"), qname);
			}
			strbuf_reset(&directory);
		} else {
			if (pathspec && !matches)
				continue;
			qname = quote_path_relative(ent->name, -1, &buf, prefix);
			if (show_only) {
				printf(_("Would remove %s\n"), qname);
				continue;
			}
			if (backup) {
				backup_file(ent->name, &st);
			}
			if (!quiet) {
				printf(_("Removing %s\n"), qname);
			}
			if (unlink(ent->name) != 0) {
				warning(_("failed to remove %s"), qname);
				errors++;
			}
		}
	}
	free(seen);

	strbuf_release(&directory);
	string_list_clear(&exclude_list, 0);

	if (backup && !show_only)
		finish_backup();

	return (errors != 0);
}
