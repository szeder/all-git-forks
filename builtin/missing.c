#include "cache.h"
#include "builtin.h"
#include "diff.h"
#include "parse-options.h"
#include "revision.h"
#include "strbuf.h"

static const char * const builtin_missing_usage[] = {
	N_("git missing [options] [<dst> | <src> <dst>]"),
	NULL
};


static const char *c_commit = "";
static const char *c_reset = "";


static void print_commit(struct commit *c, int abbrev)
{
	struct strbuf buf = STRBUF_INIT;
	pp_commit_easy(CMIT_FMT_ONELINE, c, &buf);
	printf("%s%s%s: %s\n", c_commit,
	       find_unique_abbrev(c->object.sha1, abbrev),
	       c_reset, buf.buf);
	strbuf_release(&buf);
}


static void print_list(const char *heading, int num_revs,
		       struct commit_list *list, int abbrev)
{
	struct strbuf buf;
	int len;

	strbuf_init(&buf, 128);

	strbuf_addf(&buf, heading, num_revs);
	len = buf.len - 1;
	for (; len != 0; len--) {
		strbuf_addch(&buf, '-');
	}
	strbuf_addch(&buf, '\n');

	fputs(buf.buf, stdout);

	strbuf_release(&buf);

	while (list) {
		struct commit *c = list->item;
		print_commit(c, abbrev);
		list = list->next;
	}
}


int display_missing(struct commit *left, struct commit *right, int cherry_pick,
		    int abbrev)
{
	unsigned char sha1[20];
	int num_extra, num_missing;
	char symmetric[84];
	struct rev_info revs;
	const char *rev_argv[10], *base;
	int rev_argc;
	int extra_header = 0;
	int missing_header = 0;
	struct commit_list *extra = NULL;
	struct commit_list *missing = NULL;

	/* are we the same? */
	if (left == right) {
		return 1;
	}

	/* Run "rev-list --left-right ours...theirs" internally... */
	rev_argc = 0;
	rev_argv[rev_argc++] = NULL;
	rev_argv[rev_argc++] = "--left-right";
	if (cherry_pick)
		rev_argv[rev_argc++] = "--cherry-pick";
	rev_argv[rev_argc++] = symmetric;
	rev_argv[rev_argc++] = "--";
	rev_argv[rev_argc] = NULL;

	strcpy(symmetric, sha1_to_hex(left->object.sha1));
	strcpy(symmetric + 40, "...");
	strcpy(symmetric + 43, sha1_to_hex(right->object.sha1));

	init_revisions(&revs, NULL);
	setup_revisions(rev_argc, rev_argv, &revs, NULL);
	prepare_revision_walk(&revs);

	/* ... and count the commits on each side. */
	num_extra = 0;
	num_missing = 0;
	while (1) {
		struct commit *c = get_revision(&revs);
		if (!c)
			break;
		if (c->object.flags & SYMMETRIC_LEFT) {
			commit_list_insert(c, &extra);
			num_extra++;
		}
		else {
			commit_list_insert(c, &missing);
			num_missing++;
		}
	}

	/* clear object flags smudged by the above traversal */
	clear_commit_marks(left, ALL_REV_FLAGS);
	clear_commit_marks(right, ALL_REV_FLAGS);

	if (num_extra) {
		print_list(_("You have %d extra revisions\n"), num_extra,
			   extra, abbrev);
	}
	if (num_missing) {
		if (num_extra)
			fputs("\n\n", stdout);
		print_list(_("You have %d missing revisions\n"), num_missing,
			   missing, abbrev);
	}

	fputc('\n', stdout);

	return 1;
}


struct commit *resolve_ref(const char *name)
{
	struct commit *c = lookup_commit_reference_by_name(name);
	if (!c)
		die(_("bad revision '%s'"), name);
	return c;
}


int cmd_missing(int argc, const char **argv, const char *prefix)
{
	int cherry_pick = 0;
	int abbrev = DEFAULT_ABBREV;
	struct commit *src, *dst;
	struct diff_options diffopt;
	struct option local_opts[] = {
		OPT__ABBREV(&abbrev),
		OPT_BOOL(0, "cherry-pick", &cherry_pick,
			N_("Don't count cherry-picked revisions that are on both branches")),
		OPT_END()
	};

	argc = parse_options(argc, argv, prefix, local_opts, NULL, 0);

	if (argc > 2)
		usage_msg_opt(_("Too many arguments."),
			builtin_missing_usage, local_opts);

	diffcore_std(&diffopt);
	c_commit = diff_get_color(1, DIFF_COMMIT);
	c_reset = diff_get_color(1, DIFF_RESET);

	switch (argc)
	{
	case 0:
		src = resolve_ref("HEAD");
		dst = resolve_ref("@{u}");
		break;
	case 1:
		src = resolve_ref("HEAD");
		dst = resolve_ref(argv[0]);
		break;
	case 2:
		src = resolve_ref(argv[0]);
		dst = resolve_ref(argv[1]);
		break;
	default:
		usage_msg_opt(_("Too many arguments."),
			builtin_missing_usage, local_opts);
		break;
	}

	display_missing(src, dst, cherry_pick, abbrev);

	return 0;
}
