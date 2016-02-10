/*
 * Combine packs falling in a watermark
 */

#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "run-command.h"
#include "sigchain.h"
#include "string-list.h"
#include "lockfile.h"
#include "gc.h"

#define EMPTY_PACK_SIZE 32

static const char * const builtin_combine_pack_usage[] = {
	N_("git combine-pack [options]"),
	NULL
};
static char *packdir;
static char *packtmp;
static int verbosity = 0;

static void remove_redundant_pack(const char *dir_name, const char *base_name)
{
	const char *exts[] = {".pack", ".idx", ".keep", ".bitmap"};
	int i;
	struct strbuf buf = STRBUF_INIT;
	size_t plen;

	strbuf_addf(&buf, "%s/%s", dir_name, base_name);
	plen = buf.len;

	for (i = 0; i < ARRAY_SIZE(exts); i++) {
		strbuf_setlen(&buf, plen);
		strbuf_addstr(&buf, exts[i]);
		unlink(buf.buf);
	}
	strbuf_release(&buf);
}
/* End duplicated code. */

static off_t size_lower_bound = 0;
static off_t size_upper_bound = 0;

static int rangep(off_t lower, off_t upper, off_t value) {
	if (lower > 0 && upper > 0)
		return value >= lower && value <= upper;
	else if (lower > 0)
		return value >= lower;
	else if (upper > 0)
		return value <= upper;
	else
		return 1;
}

static void list_pack_contents(struct packed_git *p, const int to)
{
	int i;
	const unsigned char *sha1 = NULL;
	char out[41];

	if (open_pack_index(p) != 0)
		die_errno("failed to open pack index for %s", sha1_to_hex(p->sha1));
	out[40] = '\n';
	for (i = 0; i < p->num_objects; i++) {
		sha1 = nth_packed_object_sha1(p, i);
		if (!sha1)
			die("internal error pack-check nth-packed-object");
		memcpy(out, sha1_to_hex(sha1), 40);
		write_in_full(to, out, 41);

	}
	close_pack_index(p);
}

static int count_packs(void)
{
	struct packed_git *p;
	int cnt=0;

	/* reject empty packs */
	off_t lower_bound = (size_lower_bound > EMPTY_PACK_SIZE) ? size_lower_bound : EMPTY_PACK_SIZE+1;

	for (p = packed_git; p; p = p->next) {
		if (!p->pack_local)
			continue;
		if (p->pack_keep)
			continue;
		if (!rangep(lower_bound, size_upper_bound, p->pack_size))
			continue;
		cnt++;
	}

	return cnt;
}

static void iter_pack(const int to, struct string_list *target_packs)
{
	struct packed_git *p;

	for (p = packed_git; p; p = p->next) {
		if (!p->pack_local)
			continue;
		if (p->pack_keep)
			continue;
		if (!rangep(size_lower_bound, size_upper_bound, p->pack_size))
			continue;
		if (verbosity > 0)
			printf("Adding objects from pack %s\n", sha1_to_hex(p->sha1));
		list_pack_contents(p, to);
		string_list_append(target_packs, sha1_to_hex(p->sha1));
	}
}

int cmd_combine_pack(int argc, const char **argv, const char *prefix)
{
	pid_t pid;
	const char *lock_holder_name = lock_repo_for_gc(0, &pid);
	struct string_list_item *item;
	struct strbuf old_name = STRBUF_INIT;
	struct strbuf new_name = STRBUF_INIT;
	/* Enumerate objects in the targeted packs to `pack-objects`, creating a new pack */
	int ret = 0;
	struct strbuf tmp_pack_path = STRBUF_INIT;
	struct child_process pack_process = {0};

	const char *pack_process_argv[] = {
			"pack-objects", "--reuse-delta", "--reuse-object", "--preindex-packs", NULL,  NULL };

	struct option builtin_read_pack_options[] = {
		OPT__VERBOSITY(&verbosity),
		OPT_INTEGER(0, "size-lower-bound", &size_lower_bound, N_("lower bound of pack size")),
		OPT_INTEGER(0, "size-upper-bound", &size_upper_bound, N_("upper bound of pack size")),
		OPT_END()
	};
	struct string_list target_packs = STRING_LIST_INIT_DUP;
	struct string_list new_pack_names = STRING_LIST_INIT_DUP;
	struct strbuf line = STRBUF_INIT;
	FILE *out;

	if (lock_holder_name) {
		warning("Bailing out, GC is already running (%"PRIuMAX" @ %s).",
				(uintmax_t)pid, lock_holder_name);
		return 0;
	}

	if (argc == 2 && !strcmp(argv[1], "-h"))
		usage_with_options(builtin_combine_pack_usage, builtin_read_pack_options);
	argc = parse_options(argc, argv, prefix, builtin_read_pack_options,
				builtin_combine_pack_usage, 0);
	if (argc > 0)
		usage_with_options(builtin_combine_pack_usage, builtin_read_pack_options);

	prepare_packed_git();
	if (count_packs() <= 1) {
		warning("Not enough packs exist to combine.");
		return 0;
	}

	packdir = mkpathdup("%s/pack", get_object_directory());
	packtmp = mkpathdup("%s/.tmp-%d-pack", packdir, (int)getpid());
	pack_process_argv[4] = packtmp;
	sigchain_push_common(remove_pack_on_signal);

	pack_process.argv = pack_process_argv;
	pack_process.in = -1;
	pack_process.out = -1;
	pack_process.err = 0;
	pack_process.git_cmd = 1;
	if (start_command(&pack_process) < 0)
		die_errno("failed to start pack-objects");

	iter_pack(pack_process.in, &target_packs);
	close(pack_process.in);

	out = xfdopen(pack_process.out, "r");
	while (strbuf_getline(&line, out) != EOF) {
		if (line.len != 40)
			die("repack: Expecting 40 character sha1 lines only from pack-objects.");
		string_list_append(&new_pack_names, line.buf);
	}
	fclose(out);
	strbuf_release(&line);

	ret = finish_command(&pack_process);
	if (ret != 0)
		die("pack-objects failed (%d)", ret);

	/* Move the new packs into place */
	for_each_string_list_item(item, &new_pack_names) {
		size_t old_name_len, new_name_len;
		strbuf_addf(&old_name, "%s-%s", packtmp, item->string);
		old_name_len = old_name.len;
		strbuf_addf(&new_name, "%s/pack-%s", packdir, item->string);
		new_name_len = new_name.len;

		/* Move .pack */
		strbuf_addf(&old_name, ".pack");
		strbuf_addf(&new_name, ".pack");
		if (rename(old_name.buf, new_name.buf)) {
			die_errno("failed to move %s -> %s",
					old_name.buf, new_name.buf);
		} else {
			trace_printf("Moving %s -> %s\n",
					old_name.buf, new_name.buf);

		}
		/* Move .idx */
		strbuf_setlen(&old_name, old_name_len);
		strbuf_setlen(&new_name, new_name_len);
		strbuf_addf(&old_name, ".idx");
		strbuf_addf(&new_name, ".idx");
		if (rename(old_name.buf, new_name.buf)) {
			die_errno("failed to move %s -> %s",
					old_name.buf, new_name.buf);
		} else if (verbosity > 0) {
			printf("Moving %s -> %s\n",
					old_name.buf, new_name.buf);
		}
	}

	/* Delete the packs containing now-duplicate objects */
	for_each_string_list_item(item, &target_packs) {
		strbuf_setlen(&old_name, 0);
		strbuf_addf(&old_name, "pack-%s", item->string);
		if (verbosity > 0)
			printf("Removing redundant pack %s\n", old_name.buf);

		remove_redundant_pack(packdir, old_name.buf);
	}
	strbuf_release(&old_name);
	strbuf_release(&new_name);
	string_list_clear(&target_packs, 0);
	string_list_clear(&new_pack_names, 0);
	strbuf_release(&tmp_pack_path);
	return ret;
}
