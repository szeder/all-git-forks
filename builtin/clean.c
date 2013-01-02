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
#include "refs.h"
#include "string-list.h"
#include "quote.h"

static int force = -1; /* unset */

static const char *const builtin_clean_usage[] = {
	N_("git clean [-d] [-f] [-n] [-q] [-e <pattern>] [-x | -X] [--] <paths>..."),
	NULL
};

static const char *MSG_REMOVE = N_("Removing %s\n");
static const char *MSG_WOULD_REMOVE = N_("Would remove %s\n");
/* static const char *MSG_WOULD_NOT_REMOVE = N_("Would not remove %s\n"); unused??? */
static const char *MSG_WOULD_IGNORE_GIT_DIR = N_("Would ignore untracked git repository %s\n");
static const char *MSG_WARN_GIT_DIR_IGNORE = N_("ignoring untracked git repository %s");
static const char *MSG_WARN_REMOVE_FAILED = N_("failed to remove %s");

static int git_clean_config(const char *var, const char *value, void *cb)
{
	if (!strcmp(var, "clean.requireforce"))
		force = !git_config_bool(var, value);
	return git_default_config(var, value, cb);
}

static int exclude_cb(const struct option *opt, const char *arg, int unset)
{
	struct string_list *exclude_list = opt->value;
	string_list_append(exclude_list, arg);
	return 0;
}

static int remove_dirs(struct strbuf *path, const char *prefix, int force_flag,
		int dry_run, int quiet, int *dir_gone)
{
	DIR *dir;
	struct strbuf quoted = STRBUF_INIT;
	struct dirent *e;
	int res = 0, ret = 0, gone = 1, original_len = path->len, len, i;
	unsigned char submodule_head[20];
	struct string_list dels = STRING_LIST_INIT_DUP;

	*dir_gone = 1;

	quote_path_relative(path->buf, strlen(path->buf), &quoted, prefix);
	if ((force_flag & REMOVE_DIR_KEEP_NESTED_GIT) &&
	    !resolve_gitlink_ref(path->buf, "HEAD", submodule_head)) {
		if (dry_run && !quiet)
			printf(_(MSG_WOULD_IGNORE_GIT_DIR), quoted.buf);
		else if (!dry_run)
			warning(_(MSG_WARN_GIT_DIR_IGNORE), quoted.buf);

		*dir_gone = 0;
		return 0;
	}

	dir = opendir(path->buf);
	if (!dir) {
		/* an empty dir could be removed even if it is unreadble */
		res = dry_run ? 0 : rmdir(path->buf);
		if (res) {
			warning(_(MSG_WARN_REMOVE_FAILED), quoted.buf);
			*dir_gone = 0;
		}
		return res;
	}

	if (path->buf[original_len - 1] != '/')
		strbuf_addch(path, '/');

	len = path->len;
	while ((e = readdir(dir)) != NULL) {
		struct stat st;
		if (is_dot_or_dotdot(e->d_name))
			continue;

		strbuf_setlen(path, len);
		strbuf_addstr(path, e->d_name);
		quote_path_relative(path->buf, strlen(path->buf), &quoted, prefix);
		if (lstat(path->buf, &st))
			; /* fall thru */
		else if (S_ISDIR(st.st_mode)) {
			if (remove_dirs(path, prefix, force_flag, dry_run, quiet, &gone))
				ret = 1;
			if (gone)
				string_list_append(&dels, quoted.buf);
			else
				*dir_gone = 0;
			continue;
		} else {
			res = dry_run ? 0 : unlink(path->buf);
			if (!res)
				string_list_append(&dels, quoted.buf);
			else {
				warning(_(MSG_WARN_REMOVE_FAILED), quoted.buf);
				*dir_gone = 0;
				ret = 1;
			}
			continue;
		}

		/* path too long, stat fails, or non-directory still exists */
		*dir_gone = 0;
		ret = 1;
		break;
	}
	closedir(dir);

	strbuf_setlen(path, original_len);
	quote_path_relative(path->buf, strlen(path->buf), &quoted, prefix);

	if (*dir_gone) {
		res = dry_run ? 0 : rmdir(path->buf);
		if (!res)
			*dir_gone = 1;
		else {
			warning(_(MSG_WARN_REMOVE_FAILED), quoted.buf);
			*dir_gone = 0;
			ret = 1;
		}
	}

	if (!*dir_gone && !quiet) {
		for (i = 0; i < dels.nr; i++)
			printf(dry_run ?  _(MSG_WOULD_REMOVE) : _(MSG_REMOVE), dels.items[i].string);
	}
	string_list_clear(&dels, 0);
	return ret;
}

int cmd_clean(int argc, const char **argv, const char *prefix)
{
	int i, res;
	int dry_run = 0, remove_directories = 0, quiet = 0, ignored = 0;
	int ignored_only = 0, config_set = 0, errors = 0, gone = 1;
	int rm_flags = REMOVE_DIR_KEEP_NESTED_GIT;
	struct strbuf directory = STRBUF_INIT;
	struct dir_struct dir;
	static const char **pathspec;
	struct strbuf buf = STRBUF_INIT;
	struct string_list exclude_list = STRING_LIST_INIT_NODUP;
	const char *qname;
	char *seen = NULL;
	struct option options[] = {
		OPT__QUIET(&quiet, N_("do not print names of files removed")),
		OPT__DRY_RUN(&dry_run, N_("dry run")),
		OPT__FORCE(&force, N_("force")),
		OPT_BOOLEAN('d', NULL, &remove_directories,
				N_("remove whole directories")),
		{ OPTION_CALLBACK, 'e', "exclude", &exclude_list, N_("pattern"),
		  N_("add <pattern> to ignore rules"), PARSE_OPT_NONEG, exclude_cb },
		OPT_BOOLEAN('x', NULL, &ignored, N_("remove ignored files, too")),
		OPT_BOOLEAN('X', NULL, &ignored_only,
				N_("remove only ignored files")),
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

	if (!dry_run && !force) {
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
			if (remove_directories || (matches == MATCHED_EXACTLY)) {
				if (remove_dirs(&directory, prefix, rm_flags, dry_run, quiet, &gone))
					errors++;
				if (gone && !quiet)
					printf(dry_run ? _(MSG_WOULD_REMOVE) : _(MSG_REMOVE), qname);
			}
			strbuf_reset(&directory);
		} else {
			if (pathspec && !matches)
				continue;
			qname = quote_path_relative(ent->name, -1, &buf, prefix);
			res = dry_run ? 0 : unlink(ent->name);
			if (res) {
				warning(_(MSG_WARN_REMOVE_FAILED), qname);
				errors++;
			} else if (!quiet)
				printf(dry_run ? _(MSG_WOULD_REMOVE) : _(MSG_REMOVE), qname);
		}
	}
	free(seen);

	strbuf_release(&directory);
	string_list_clear(&exclude_list, 0);
	return (errors != 0);
}
