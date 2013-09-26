#include "cache.h"
#include "builtin.h"
#include "parse-options.h"
#include "revision.h"
#include "refs.h"
#include "remote.h"
#include "transport.h"
#include "branch.h"
#include "shortlog.h"
#include "diff.h"
#include "log-tree.h"

static const char *tag_name;

static const char *const pull_request_usage[] = {
	N_("git request-pull [options] start url [end]"),
	NULL
};

static int describe_cb(const char *name, const unsigned char *sha1, int flags, void *cb_data)
{
	unsigned char peel_sha1[20];
	if (prefixcmp(name, "refs/tags/"))
		return 0;
	peel_ref(name, peel_sha1);
	if (hashcmp(peel_sha1, cb_data))
		return 0;
	tag_name = skip_prefix(name, "refs/tags/");
	return 1;
}

const char *describe(const char *head)
{
	unsigned char sha1[20];
	get_sha1(head, sha1);
	for_each_ref(describe_cb, sha1);
	return tag_name;
}

const char *abbr(const char *name)
{
	return name + (
			!prefixcmp(name, "refs/heads/") ? 11 :
			!prefixcmp(name, "refs/tags/") ? 5 :
			0);
}

const char *get_ref(struct transport *transport, const char *head_ref, const unsigned char *head_id)
{
	const struct ref *refs, *e;
	const char *found = NULL;
	int deref;

	refs = transport_get_remote_refs(transport);

	for (e = refs; e; e = e->next) {
		if (hashcmp(e->old_sha1, head_id))
			continue;

		deref = !suffixcmp(e->name, "^{}");
		found = abbr(e->name);
		if (deref && tag_name && !prefixcmp(e->name + 10, tag_name))
			break;
		if (head_ref && !strcmp(e->name, head_ref))
			break;
	}
	if (!found)
		return NULL;
	return xstrndup(found, strlen(found) - (deref ? 3 : 0));
}

static const char *show_ident_date(const struct ident_split *ident,
				   enum date_mode mode)
{
	unsigned long date = 0;
	int tz = 0;

	if (ident->date_begin && ident->date_end)
		date = strtoul(ident->date_begin, NULL, 10);
	if (ident->tz_begin && ident->tz_end)
		tz = strtol(ident->tz_begin, NULL, 10);
	return show_date(date, tz, mode);
}

static void parse_buffer(const char *buffer, const char **summary, const char **date)
{
	struct strbuf subject = STRBUF_INIT;
	struct ident_split s;
	const char *c, *e;

	c = strstr(buffer, "\ncommitter ") + 11;
	e = strchr(c, '\n');
	if (!split_ident_line(&s, c, e - c))
		*date = show_ident_date(&s, DATE_ISO8601);

	c = strstr(c, "\n\n") + 2;
	format_subject(&subject, c, " ");
	*summary = strbuf_detach(&subject, NULL);
}

static void show_shortlog(const char *base, const char *head)
{
	struct commit *commit;
	struct rev_info revs;
	struct shortlog log;
	const char *args[3];
	struct strbuf tmp = STRBUF_INIT;

	strbuf_addf(&tmp, "^%s", base);

	args[1] = tmp.buf;
	args[2] = head;

	init_revisions(&revs, NULL);
	setup_revisions(3, args, &revs, NULL);

	strbuf_release(&tmp);

	shortlog_init(&log);
	prepare_revision_walk(&revs);
	while ((commit = get_revision(&revs)))
		shortlog_add_commit(&log, commit);
	shortlog_output(&log);
}

static void show_diff(int patch, const unsigned char *base, const unsigned char *head)
{
	struct rev_info revs;
	const char *args[3];
	struct strbuf tmp = STRBUF_INIT;

	strbuf_addf(&tmp, "^%s", sha1_to_hex(base));

	args[1] = tmp.buf;
	args[2] = sha1_to_hex(head);

	init_revisions(&revs, NULL);
	setup_revisions(3, args, &revs, NULL);
	revs.diffopt.stat_width = -1;
	revs.diffopt.stat_graph_width = -1;
	revs.diffopt.output_format = patch ? DIFF_FORMAT_PATCH : DIFF_FORMAT_DIFFSTAT;
	revs.diffopt.output_format |= DIFF_FORMAT_SUMMARY;
	revs.diffopt.detect_rename = DIFF_DETECT_RENAME;
	revs.diffopt.flags |= DIFF_OPT_RECURSIVE;

	strbuf_release(&tmp);
	diff_tree_sha1(base, head, "", &revs.diffopt);
	log_tree_diff_flush(&revs);
}

int cmd_request_pull(int argc, const char **argv, const char *prefix)
{
	int patch;
	const char *base, *url, *head;
	char *head_ref;
	char *branch_name = NULL;
	struct strbuf branch_desc = STRBUF_INIT;
	const char *ref;
	int status = 0;
	unsigned char head_id[20], base_id[20];
	unsigned char *merge_base_id;
	struct commit *base_commit, *head_commit, *merge_base_commit;
	struct commit_list *merge_bases;
	const char *merge_base_summary, *merge_base_date;
	const char *head_summary, *head_date;
	struct remote *remote;
	struct transport *transport;

	const struct option options[] = {
		OPT_BOOL('p', NULL, &patch, N_("show patch text as well")),
		OPT_END(),
	};

	argc = parse_options(argc, argv, prefix, options, pull_request_usage, 0);

	base = argv[0];
	url = argv[1];
	head = argv[2] ? argv[2] : "HEAD";
	status = 0;

	if (!base || !url)
		usage_with_options(pull_request_usage, options);

	if (dwim_ref(head, strlen(head), head_id, &head_ref) == 1) {
		if (!prefixcmp(head_ref, "refs/heads/")) {
			branch_name = head_ref + 11;
			if (read_branch_desc(&branch_desc, branch_name) || !branch_desc.len)
				branch_name = NULL;
		}
	}

	describe(head);

	get_sha1(base, base_id);
	base_commit = lookup_commit_reference(base_id);
	if (!base_commit)
		die("Not a valid revision: %s", base);

	head_commit = lookup_commit_reference(head_id);
	if (!head_commit)
		die("Not a valid revision: %s", head);

	merge_bases = get_merge_bases(base_commit, head_commit, 0);
	if (!merge_bases)
		die("No commits in common between %s and %s", base, head);

	merge_base_commit = merge_bases->item;
	merge_base_id = merge_base_commit->object.sha1;

	remote = remote_get(url);
	url = remote->url[0];
	transport = transport_get(remote, url);
	ref = get_ref(transport, strcmp(head_ref, "HEAD") ? head_ref : NULL, head_id);
	transport_disconnect(transport);

	parse_buffer(merge_base_commit->buffer, &merge_base_summary, &merge_base_date);
	parse_buffer(head_commit->buffer, &head_summary, &head_date);

	printf("The following changes since commit %s:\n"
			"\n"
			"  %s (%s)\n"
			"\n"
			"are available in the git repository at:\n"
			"\n",
			sha1_to_hex(merge_base_id), merge_base_summary, merge_base_date);
	printf("  %s", url);
	if (ref)
		printf(" %s", ref);
	printf("\n");
	printf("\n"
			"for you to fetch changes up to %s:\n"
			"\n"
			"  %s (%s)\n"
			"\n"
			"----------------------------------------------------------------\n",
			sha1_to_hex(head_id), head_summary, head_date);

	if (branch_name)
		printf("(from the branch description for %s local branch)\n\n%s\n", branch_name, branch_desc.buf);

	if (tag_name) {
		void *buffer;
		enum object_type type;
		unsigned long size;
		unsigned char sha1[20];
		const char *begin, *end;

		if (!ref || prefixcmp(ref, "tags/") || strcmp(ref + 5, tag_name)) {
			fprintf(stderr, "warn: You locally have %s but it does not (yet)\n", tag_name);
			fprintf(stderr, "warn: appear to be at %s\n", url);
			fprintf(stderr, "warn: Do you want to push it there, perhaps?\n");
		}
		get_sha1(tag_name, sha1);
		buffer = read_sha1_file(sha1, &type, &size);
		begin = strstr(buffer, "\n\n") + 2;
		end = strstr(begin, "-----BEGIN PGP ");
		if (!end)
			end = begin + strlen(begin);
		printf("%.*s\n", (int)(end - begin), begin);
	}

	if (branch_name || tag_name)
		puts("----------------------------------------------------------------");

	show_shortlog(base, head);
	show_diff(patch, merge_base_id, head_id);

	if (!ref) {
		fprintf(stderr, "warn: No branch of %s is at:\n", url);
		fprintf(stderr, "warn:   %s: %s\n", find_unique_abbrev(head_id, DEFAULT_ABBREV), head_summary);
		fprintf(stderr, "warn: Are you sure you pushed '%s' there?\n", head);
		status = 1;
	}
	return status;
}
