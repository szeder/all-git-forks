#include "git-compat-util.h"
#include "cache.h"
#include "dir.h"
#include "pathspec.h"
#include "exec_cmd.h"
#include "parse-options.h"
#include "strbuf.h"
#include "fs_cache.h"

static int validate = 0;
static int quiet = 0;
static int skip_ignored = 0;

static char const * const fs_cache_dump_usage[] = {
	N_("git fs-cache-dump [ -v ] [ -q ] [ -g ]"),
	NULL
};

static void print_dev(const dev_t* dev) {
	printf("%" PRIuMAX, (uintmax_t)*dev);
}
static void print_ino(const ino_t* ino) {
	printf("%" PRIuMAX, (uintmax_t)*ino);
}
static void print_mode(const mode_t* mode) {
	printf("%" PRIoMAX, (uintmax_t)*mode);
}
static void print_uid(const uid_t* uid) {
	printf("%" PRIuMAX, (uintmax_t)*uid);
}
static void print_gid(const gid_t* gid) {
	printf("%" PRIuMAX, (uintmax_t)*gid);
}
static void print_mtime(const time_t* mtime) {
	printf("%" PRIuMAX, (uintmax_t)mtime);
}
static void print_ctime(const time_t* ctime) {
	printf("%" PRIuMAX, (uintmax_t)ctime);
}
static void print_size(const off_t* size) {
	printf("%" PRIdMAX, (intmax_t)*size);
}

#define STAT_FIELD_DO(macro) \
	macro(dev) \
	macro(ino) \
	macro(mode) \
	macro(uid) \
	macro(gid) \
	macro(mtime) \
	macro(ctime) \
	macro(size)

#define CHECK_STAT_FIELD(field) \
	if (memcmp(&fse->st.st_##field, &fs_buf.st_##field, sizeof(fs_buf.st_##field))) { \
		printf("** VALIDATION FAILURE:\n"); \
		printf("    fs-cache stat data does not match what is on filesystem\n"); \
		printf("    path: \"%s\"\n", fse->path); \
		printf("    fs-cache   %s: ", #field); \
		print_##field(&fse->st.st_##field); \
		printf("\n    filesystem %s: ", #field); \
		print_##field(&fs_buf.st_##field); \
		printf("\n\n"); \
		has_error = 1; \
	}

static int validate_fs_entry(const struct fsc_entry* fse) {
	struct stat fs_buf;
	int has_error = 0;

	if (fse->path[0] != '\0') {
		int res = lstat(fse->path, &fs_buf);
		if (res == 0) {
			if (S_ISDIR(fse->st.st_mode)) {
				CHECK_STAT_FIELD(mode);
			} else {
				STAT_FIELD_DO(CHECK_STAT_FIELD);
			}
		} else {
			printf("** VALIDATION FAILURE:\n");
			printf("    Item does not exist on filesystem.\n");
			printf("    path: \"%s\"\n", fse->path);
			has_error = 1;
		}
	}
	return has_error;
}

#undef CHECK_STAT_FIELD
#undef STAT_FIELD_DO

static int fs_printf(const char* fmt, ...) {
	int status = 0;
	va_list va;
	if (!quiet) {
		va_start(va, fmt);
		status = vprintf(fmt, va);
		va_end(va);
	}
	return status;
}

int fs_cache_dump(void)
{
	struct hashmap_iter iter;
	struct fsc_entry *fse;
	struct dir_struct dir;
	int has_error = 0;

	fs_printf("last_update=%s\n", the_fs_cache.last_update);
	fs_printf("repo_path=%s\n", the_fs_cache.repo_path);
	fs_printf("excludes_file=%s\n", the_fs_cache.excludes_file);
	fs_printf("git_excludes_sha1=%s\n", sha1_to_hex(the_fs_cache.git_excludes_sha1));
	fs_printf("user_excludes_sha1=%s\n", sha1_to_hex(the_fs_cache.user_excludes_sha1));
	fs_printf("version=%u\n", the_fs_cache.version);
	fs_printf("nr=%d\n", the_fs_cache.nr);
	fs_printf("fully_loaded=%u\n", the_fs_cache.fully_loaded);
	fs_printf("needs_write=%u\n", the_fs_cache.needs_write);
	fs_printf("flags=%04x\n", the_fs_cache.flags);
	fs_printf("\n\n");

	if (skip_ignored) {
		memset(&dir, 0, sizeof(dir));
		setup_standard_excludes(&dir);
	}

	hashmap_iter_init(&the_fs_cache.paths, &iter);
	while ((fse = hashmap_iter_next(&iter))) {
		int dtype = DT_UNKNOWN;
		if (!skip_ignored || !is_excluded(&dir, fse->path, &dtype)) {
			fs_printf("%.*s\t%d\t%o\n", fse->pathlen, fse->path,
			    fse->in_index, fse->st.st_mode);
			if (validate) {
				has_error |= validate_fs_entry(fse);
			}
		}
	}
	return has_error;
}

int main(int argc, const char **argv)
{
	struct option opts[] = {
		OPT_SET_INT('v', NULL, &validate,
				N_("Validate contents of fs-cache against filesystem"), 'v'),
		OPT_SET_INT('q', NULL, &quiet,
				N_("Inhibits display of dump information to stdout"), 'q'),
		OPT_SET_INT('g', NULL, &skip_ignored,
				N_("Skip printing and validating items that are in .gitignore"), 'g'),
		OPT_END()
	};

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	argc = parse_options(argc, argv, NULL, opts, fs_cache_dump_usage, 0);
	git_config(git_default_config, NULL);

	if (!read_fs_cache()) {
		die("Could not read fs_cache\n");
	}

	return fs_cache_dump();
}
