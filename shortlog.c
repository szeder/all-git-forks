#include "cache.h"
#include "shortlog.h"
#include "commit.h"
#include "mailmap.h"
#include "utf8.h"

void shortlog_init(struct shortlog *log)
{
	memset(log, 0, sizeof(*log));

	read_mailmap(&log->mailmap, &log->common_repo_prefix);

	log->list.strdup_strings = 1;
	log->wrap = DEFAULT_WRAPLEN;
	log->in1 = DEFAULT_INDENT1;
	log->in2 = DEFAULT_INDENT2;
}

void shortlog_insert_one_record(struct shortlog *log,
		const char *author,
		const char *oneline)
{
	const char *dot3 = log->common_repo_prefix;
	char *buffer, *p;
	struct string_list_item *item;
	const char *mailbuf, *namebuf;
	size_t namelen, maillen;
	const char *eol;
	struct strbuf subject = STRBUF_INIT;
	struct strbuf namemailbuf = STRBUF_INIT;
	struct ident_split ident;

	if (split_ident_line(&ident, author, strlen(author)))
		return;

	namebuf = ident.name_begin;
	mailbuf = ident.mail_begin;
	namelen = ident.name_end - ident.name_begin;
	maillen = ident.mail_end - ident.mail_begin;

	map_user(&log->mailmap, &mailbuf, &maillen, &namebuf, &namelen);
	strbuf_add(&namemailbuf, namebuf, namelen);

	if (log->email)
		strbuf_addf(&namemailbuf, " <%.*s>", (int)maillen, mailbuf);

	item = string_list_insert(&log->list, namemailbuf.buf);
	if (item->util == NULL)
		item->util = xcalloc(1, sizeof(struct string_list));

	/* Skip any leading whitespace, including any blank lines. */
	while (*oneline && isspace(*oneline))
		oneline++;
	eol = strchr(oneline, '\n');
	if (!eol)
		eol = oneline + strlen(oneline);
	if (!prefixcmp(oneline, "[PATCH")) {
		char *eob = strchr(oneline, ']');
		if (eob && (!eol || eob < eol))
			oneline = eob + 1;
	}
	while (*oneline && isspace(*oneline) && *oneline != '\n')
		oneline++;
	format_subject(&subject, oneline, " ");
	buffer = strbuf_detach(&subject, NULL);

	if (dot3) {
		int dot3len = strlen(dot3);
		if (dot3len > 5) {
			while ((p = strstr(buffer, dot3)) != NULL) {
				int taillen = strlen(p) - dot3len;
				memcpy(p, "/.../", 5);
				memmove(p + 5, p + dot3len, taillen + 1);
			}
		}
	}

	string_list_append(item->util, buffer);
}

void shortlog_add_commit(struct shortlog *log, struct commit *commit)
{
	const char *author = NULL, *buffer;
	struct strbuf buf = STRBUF_INIT;
	struct strbuf ufbuf = STRBUF_INIT;

	pp_commit_easy(CMIT_FMT_RAW, commit, &buf);
	buffer = buf.buf;
	while (*buffer && *buffer != '\n') {
		const char *eol = strchr(buffer, '\n');

		if (eol == NULL)
			eol = buffer + strlen(buffer);
		else
			eol++;

		if (!prefixcmp(buffer, "author "))
			author = buffer + 7;
		buffer = eol;
	}
	if (!author) {
		warning(_("Missing author: %s"),
		    sha1_to_hex(commit->object.sha1));
		return;
	}
	if (log->user_format) {
		struct pretty_print_context ctx = {0};
		ctx.fmt = CMIT_FMT_USERFORMAT;
		ctx.abbrev = log->abbrev;
		ctx.subject = "";
		ctx.after_subject = "";
		ctx.date_mode = DATE_NORMAL;
		ctx.output_encoding = get_log_output_encoding();
		pretty_print_commit(&ctx, commit, &ufbuf);
		buffer = ufbuf.buf;
	} else if (*buffer) {
		buffer++;
	}
	shortlog_insert_one_record(log, author, !*buffer ? "<none>" : buffer);
	strbuf_release(&ufbuf);
	strbuf_release(&buf);
}

static int compare_by_number(const void *a1, const void *a2)
{
	const struct string_list_item *i1 = a1, *i2 = a2;
	const struct string_list *l1 = i1->util, *l2 = i2->util;

	if (l1->nr < l2->nr)
		return 1;
	else if (l1->nr == l2->nr)
		return 0;
	else
		return -1;
}

static void add_wrapped_shortlog_msg(struct strbuf *sb, const char *s,
				     const struct shortlog *log)
{
	strbuf_add_wrapped_text(sb, s, log->in1, log->in2, log->wrap);
	strbuf_addch(sb, '\n');
}

void shortlog_output(struct shortlog *log)
{
	int i, j;
	struct strbuf sb = STRBUF_INIT;

	if (log->sort_by_number)
		qsort(log->list.items, log->list.nr, sizeof(struct string_list_item),
			compare_by_number);
	for (i = 0; i < log->list.nr; i++) {
		struct string_list *onelines = log->list.items[i].util;

		if (log->summary) {
			printf("%6d\t%s\n", onelines->nr, log->list.items[i].string);
		} else {
			printf("%s (%d):\n", log->list.items[i].string, onelines->nr);
			for (j = onelines->nr - 1; j >= 0; j--) {
				const char *msg = onelines->items[j].string;

				if (log->wrap_lines) {
					strbuf_reset(&sb);
					add_wrapped_shortlog_msg(&sb, msg, log);
					fwrite(sb.buf, sb.len, 1, stdout);
				}
				else
					printf("      %s\n", msg);
			}
			putchar('\n');
		}

		onelines->strdup_strings = 1;
		string_list_clear(onelines, 0);
		free(onelines);
		log->list.items[i].util = NULL;
	}

	strbuf_release(&sb);
	log->list.strdup_strings = 1;
	string_list_clear(&log->list, 1);
	clear_mailmap(&log->mailmap);
}
