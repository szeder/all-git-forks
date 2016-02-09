#include "git-compat-util.h"
#include "cache.h"
#include "exec_cmd.h"
#include "journal.h"
#include "object.h"
#include "refs.h"
#include "journal-connectivity.h"
#include "string-list.h"
#include "lockfile.h"

static char const * const journal_usage = N_(
	"git journal-append <command> <command_args...>\n"
	"  - commands:\n"
	"    auto <refname> <sha>  Append commit/ref and, if necessary, its pack\n"
	"    ref  <refname> <sha>  Append a ref\n"
	"    pack <sha>            Append a pack\n"
	"    tips                 'auto' en masse from stdin (format: ref<sp>sha)\n"
	"    upgrade               Append an upgrade notification\n");

static unsigned long journal_size_limit;

/* packs larger than this many bytes may not be added
 * 0 is 'unset' so to have any effect this must be >= 1 */
static size_t journal_max_pack_size;

static void journal_append_pack(struct journal_ctx *c, struct packed_git *pack)
{
	enum pack_check_result result;

	if (!pack)
		die("pack not found '%s'",
		    sha1_to_hex(pack->sha1));

	trace_printf("Append [pack] %s\n",
		     sha1_to_hex(pack->sha1));

	result = jcdb_check_and_record_pack(pack);
	switch(result) {
	case PACK_PRESENT:
		warning("ignoring previously journaled pack %s",
			sha1_to_hex(pack->sha1));
		break;
	case PACK_ADDED:
		journal_write_pack(c, pack, journal_max_pack_size);
		break;
	case PACK_INVALID:
		die("Invalid pack %s\n", sha1_to_hex(pack->sha1));
	default:
		die("BUG: unexpected result %d from jcdb_check_and_record_pack",
		    result);
	}
}

static void journal_append_auto_or_ref(struct journal_ctx *c, const char *ref_name, const unsigned char *commit_sha1)
{
	int type;
	unsigned char old_sha1[20];
	int have_old = 1;
	struct packed_git *pack;

	if (read_ref(ref_name, old_sha1)) {
		memset(old_sha1, 0, 20);
		have_old = 0;
	}

	if (is_null_sha1(commit_sha1)) {
		/* Ref deletion? */
		if (!have_old)
			die("No old ref %s", ref_name);
		journal_write_tip(c, ref_name, commit_sha1);
		trace_printf("Append [rm-ref] %s\n",
			     ref_name);
		jcdb_record_update_ref(old_sha1, commit_sha1);
		return;
	}
	type = sha1_object_info(commit_sha1, NULL);
	trace_printf("Append [%s] %s (%s)\n",
		     typename(type), sha1_to_hex(commit_sha1), ref_name);

	if (type <= OBJ_NONE)
		die("object %s not found",
		    sha1_to_hex(commit_sha1));

	pack = journal_locate_pack_by_sha(commit_sha1);
	if (!pack) {
		die("could not locate pack for commit %s", sha1_to_hex(commit_sha1));
	}

	journal_append_pack(c, pack);
	journal_write_tip(c, ref_name, commit_sha1);

	jcdb_record_update_ref(old_sha1, commit_sha1);
}

static void journal_append_upgrade(struct journal_ctx *c, const struct journal_wire_version *v)
{
	printf("Writing UPGRADE op: %s", sha1_to_hex((unsigned char *)v));
	journal_write_upgrade(c, v);
}

static int journal_config(const char *var, const char *value, void *cb)
{
	if (strcmp(var, "receive.unpacklimit") == 0) {
		int receive_unpack_limit = git_config_int(var, value);

		if (receive_unpack_limit != 1) {
			error("journalling: receive.unpacklimit must be set to `1`");
			return 1;
		}

		return 0;
	}

	if (strcmp(var, "transfer.unpacklimit") == 0) {
		int transfer_unpack_limit = git_config_int(var, value);

		if (transfer_unpack_limit != 1) {
			die("journalling: transfer.unpacklimit must be set to `1`");
			return 1;
		}

		return 0;
	}

	if (strcmp(var, "receive.autogc") == 0) {
		int auto_gc = git_config_bool(var, value);
		if (auto_gc != 0) {
			error("journalling: receive.autogc must be set to `0`");
			return 1;
		}

		return 0;
	}

	if (strcmp(var, "journal.size-limit") == 0) {
		journal_size_limit  = git_config_ulong(var, value);
		if (journal_size_limit > UINT32_MAX) {
			error("journal size limit must be < 4G (set: %ld max: %du)",
			      journal_size_limit, UINT32_MAX);
			return 1;
		}

		return 0;
	}

	if (strcmp(var, "pack.maxpacksize") == 0) {
		/* if journal_max_pack_size is still at the default value, set it to pack.maxpacksize */
		if (journal_max_pack_size == 0) {
			journal_max_pack_size = git_config_ulong(var, value);
		}
		return 0;
	}

	if (strcmp(var, "journal.maxpacksize") == 0) {
		journal_max_pack_size = git_config_ulong(var, value);
		return 0;
	}

	return git_default_config(var, value, cb);
}

int main(int argc, const char **argv)
{
	int ret = 0, use_integrity;
	const char *cmd, *ref_name = NULL;
	unsigned char sha1[20] = {0};
	struct journal_ctx *c;

	if (argc < 2)
		goto show_usage;

	git_extract_argv0_path(argv[0]);

	setup_git_directory();

	journal_size_limit = git_config_ulong("journal.size-limit", JOURNAL_MAX_SIZE_DEFAULT);
	use_integrity = journal_integrity_from_config();
	git_config(journal_config, NULL);

	cmd = *++argv;

	c = journal_ctx_open(journal_size_limit, use_integrity);

	if (!strcmp(cmd, "auto") && argc == 4) {
		ref_name = *++argv;
		get_sha1_hex(*++argv, sha1);
		journal_append_auto_or_ref(c, ref_name, sha1);
	} else if (!strcmp(cmd, "pack") && argc == 3) {
		const char *sha1_to_parse = *++argv;

		get_sha1_hex(sha1_to_parse, sha1);
		if (is_null_sha1(sha1)) {
			warning("could not parse pack sha1 '%s'", sha1_to_parse);
			goto show_usage;
		}

		journal_append_pack(c, journal_find_pack(sha1));
	} else if (!strcmp(cmd, "ref") && argc == 4)  {
		ref_name = *++argv;
		get_sha1_hex(*++argv, sha1);
		journal_append_auto_or_ref(c, ref_name, sha1);
	} else if (!strcmp(cmd, "tips") && argc == 2) {
		struct strbuf line = STRBUF_INIT;
		struct string_list tokens = STRING_LIST_INIT_DUP;
		while (strbuf_getline(&line, stdin) != EOF) {
			char *lhs, *rhs;

			string_list_clear(&tokens, 0);
			string_list_split(&tokens, line.buf, ' ', 2);
			lhs = tokens.items[0].string;
			rhs = tokens.items[1].string;
			get_sha1_hex(rhs, sha1);
			journal_append_auto_or_ref(c, lhs, sha1);
		}
	} else if (!strcmp(cmd, "upgrade")) {
		journal_append_upgrade(c, journal_wire_version());
	} else {
		usage(journal_usage);
		ret = -1;
	}

	journal_ctx_close(c);

	return ret;

 show_usage:
	usage(journal_usage);
	return -1;
}
