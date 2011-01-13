/*
 * Produce a dumpfile v3 from a fast-import stream.
 * Load the dump into the SVN repository with:
 * svnrdump load <URL> <dumpfile
 *
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "cache.h"
#include "quote.h"
#include "svnload.h"
#include "dump_export.h"
#include "dir_cache.h"

static FILE *infile;
static struct strbuf command_buf = STRBUF_INIT;
static struct strbuf path_d = STRBUF_INIT;

static int read_next_command(void)
{
	strbuf_reset(&command_buf);
	return strbuf_getline(&command_buf, infile, '\n');
}

static void populate_revprops(struct strbuf *revprops,
			struct ident *svn_ident, struct strbuf *log)
{
	strbuf_reset(revprops);
	strbuf_addf(revprops, "K 10\nsvn:author\nV %lu\n%s\n",
		svn_ident->name.len, svn_ident->name.buf);
	strbuf_addf(revprops, "K 7\nsvn:log\nV %lu\n%s\n",
		log->len, log->buf);
	if (svn_ident->date)
		/* SVN doesn't like an empty svn:date value */
		strbuf_addf(revprops, "K 8\nsvn:date\nV %d\n%s\n",
			SVN_DATE_LEN - 1, svn_ident->date);
	strbuf_add(revprops, "PROPS-END\n", 10);
}

static void parse_ident(const char *buf, struct ident *identp)
{
	char *original_buf, *t, *tz_off;
	int tz_off_buf;
	const struct tm *tm_time;

	/* John Doe <johndoe@email.com> 1170199019 +0530 */
	strbuf_reset(&(identp->name));
	strbuf_reset(&(identp->email));

	original_buf = strdup(buf);
	if (!(tz_off = strrchr(buf, ' ')))
		goto error;
	*tz_off++ = '\0';
	if (!(t = strrchr(buf, ' ')))
		goto error;
	*(t - 1) = '\0'; /* Ignore '>' from email */
	t++;
	tz_off_buf = atoi(tz_off);

	/* UTC -1200 to UTC +1400 are valid */
	if (tz_off_buf > 1400 || tz_off_buf < -1200)
		goto error;
	tm_time = time_to_tm(strtoul(t, NULL, 10), tz_off_buf);
	strftime(identp->date, SVN_DATE_LEN, SVN_DATE_FORMAT, tm_time);
	if (!(t = strchr(buf, '<')))
		goto error;
	*(t - 1) = '\0'; /* Ignore ' <' from email */
	t++;

	strbuf_add(&(identp->email), t, strlen(t));
	strbuf_add(&(identp->name), buf, strlen(buf));
	free(original_buf);
	return;
error:
	die("Malformed ident line: %s", original_buf);
}

static void skip_optional_lf(void)
{
	int term_char = fgetc(infile);
	if (term_char != '\n' && term_char != EOF)
		ungetc(term_char, infile);
}

/* Either sets term and returns terminator length or returns data
   length after setting term to NULL */
static size_t parse_data_len(char *term)
{
	uintmax_t length;

	term = NULL;
	if (prefixcmp(command_buf.buf, "data "))
		die("Expected 'data n' command, found: %s", command_buf.buf);

	if (!prefixcmp(command_buf.buf + 5, "<<")) {
		term = xstrdup(command_buf.buf + 5 + 2);
		if (!(command_buf.len - 5 - 2))
			die("Missing delimeter after 'data <<' in: %s", command_buf.buf);
		return (size_t) (command_buf.len - 5 - 2);
	}

	length = strtoumax(command_buf.buf + 5, NULL, 10);
	if ((size_t) length < length)
		die("Data is too large to use in this context");

	return (size_t) length;
}

/* When term is filled in, nbytes refers to the size of the
   terminator; otherwise, it refers to the size of the actual data.
   The parsed data is written to dst and out, if they exist. */
static void parse_data(char *term, size_t nbytes, struct strbuf *dst, FILE *out)
{
	size_t in;
	size_t done = 0;

	if (term) {
		/* Read line-by-line until terminator is encountered */
		while (1) {
			if (read_next_command() == EOF)
				die("Expected terminator '%s', found EOF", term);

			/* If the terminator is encountered, stop reading */
			if (nbytes == command_buf.len
				&& !memcmp(term, command_buf.buf, nbytes))
				break;

			if (dst) {
				strbuf_addbuf(dst, &command_buf);
				strbuf_addch(dst, '\n');
			}
			if (out) {
				strbuf_fwrite(&command_buf, command_buf.len, out);
				fprintf(out, "\n");
			}
		}
		free(term);
		goto END;
	}

	/*  Read nbytes bytes in chunks */
	while (done < nbytes && !feof(infile) && !ferror(infile)) {
		in = (nbytes - done) < COPY_BUFFER_LEN ?
			(nbytes - done) : COPY_BUFFER_LEN;
		strbuf_reset(&command_buf);
		in = strbuf_fread(&command_buf, in, infile);
		done += in;
		if (dst)
			strbuf_addbuf(dst, &command_buf);
		if (out)
			strbuf_fwrite(&command_buf, command_buf.len, out);
	}
	if (done != nbytes)
		die("Expected %lu bytes, read %lu bytes", nbytes, done);

	if (out)
		fprintf(out, "\n"); /* Hack: Incase data is not terminated with lf */
END:
	skip_optional_lf();
	skip_optional_lf();
}

static const char *get_mode(const char *str, uint16_t *modep)
{
	unsigned char c;
	uint16_t mode = 0;

	while ((c = *str++) != ' ') {
		if (c < '0' || c > '7')
			return NULL;
		mode = (mode << 3) + (c - '0');
	}
	*modep = mode;
	return str;
}

static void file_change_m(const char *p)
{
	struct strbuf dst = STRBUF_INIT;
	const char *endp;
	uint16_t mode;
	size_t nbytes;
	char *term = NULL;

	if (!p)
		die("Missing mode after filemodify in: %s", command_buf.buf);

	if (!(p = get_mode(p, &mode)))
		die("Corrupt mode: %s", command_buf.buf);
	if (!prefixcmp(p, "inline "))
		p += 7;
	else
		die("Non-inlined data unsupported");

	/* Parse out path into path_d */
	strbuf_reset(&path_d);
	if (!unquote_c_style(&path_d, p, &endp)) {
		if (*endp)
			die("Garbage after path in: %s", command_buf.buf);
	} else
		strbuf_addstr(&path_d, p);

	read_next_command();
	nbytes = parse_data_len(term);
	if (term) {
		strbuf_reset(&dst);
		parse_data(term, nbytes, &dst, NULL);
		dump_export_m(path_d.buf, mode, dst.len);
		fwrite(&dst.buf, 1, dst.len, stdout);
		return;
	}
	dump_export_m(path_d.buf, mode, nbytes);
	parse_data(NULL, nbytes, NULL, stdout);
}

static void file_change_d(const char *p)
{
	const char *endp;

	if (!p)
		die("Missing path after filedelete in: %s", command_buf.buf);

	strbuf_reset(&path_d);
	if (!unquote_c_style(&path_d, p, &endp)) {
		if (*endp)
			die("Garbage after path in: %s", command_buf.buf);
	} else
		strbuf_addstr(&path_d, p);
	dump_export_d(path_d.buf);
}

static void file_change_cr(const char *p, int delete_old)
{
	struct strbuf path_s = STRBUF_INIT;
	const char *endp;

	if (!p)
		die("Missing source after %s in: %s",
			delete_old ? "filerename" : "filecopy", command_buf.buf);

	strbuf_reset(&path_s);
	if (!unquote_c_style(&path_s, p, &endp)) {
		if (*endp != ' ')
			die("Missing destination after source in: %s", command_buf.buf);
	} else {
		endp = strchr(p, ' ');
		if (!endp)
			die("Missing destination after source in: %s", command_buf.buf);
		strbuf_add(&path_s, p, endp - p);
	}

	endp++;
	if (!*endp)
		die("Missing destination in: %s", command_buf.buf);

	p = endp;
	strbuf_reset(&path_d);
	if (!unquote_c_style(&path_d, p, &endp)) {
		if (*endp)
			die("Garbage after destination in: %s", command_buf.buf);
	} else
		strbuf_addstr(&path_d, p);

	/* TODO: Check C "path/to/subdir" "" */
	dump_export_cr(path_d.buf, path_s.buf, delete_old);
}

static void build_svn_ident(struct ident *svn_ident,
			struct ident *author, struct ident *committer)
{
	char *t, *email;

	strbuf_reset(&(svn_ident->name));
	memcpy(svn_ident->date, committer->date, SVN_DATE_LEN);
	email = author->email.len ? author->email.buf : committer->email.buf;
	if ((t = strchr(email, '@')))
		strbuf_add(&(svn_ident->name), email, t - email);
	else
		strbuf_addstr(&(svn_ident->name), email);
}

static void parse_ignore_notemodify(const char *p)
{
	char *term;
	size_t nbytes;

	if (!p)
		die("Missing dataref after notemodify in: %s", command_buf.buf);
	if (!(p = strchr(p, ' ')))
		die ("Missing committish after dataref in: %s", command_buf.buf);

	read_next_command();
	term = NULL;
	nbytes = parse_data_len(term);
	parse_data(term, nbytes, NULL, NULL);
}

static void parse_commit(const char *p)
{
	static struct strbuf log = STRBUF_INIT;
	static struct strbuf revprops = STRBUF_INIT;
	static struct ident author = {STRBUF_INIT, STRBUF_INIT, ""};
	static struct ident committer = {STRBUF_INIT, STRBUF_INIT, ""};
	static struct ident svn_ident = {STRBUF_INIT, {0, 0, NULL}, ""};

	char *ident_buf, *term;
	size_t nbytes;

	/* TODO: Parse and use branch */
	if (!p)
		die("Missing ref after commit in: %s", command_buf.buf);
	read_next_command();

	/* Parse and ignore mark line */
	if (!prefixcmp(command_buf.buf, "mark :"))
		read_next_command();

	if (!prefixcmp(command_buf.buf, "author ")) {
		ident_buf = strbuf_detach(&command_buf, &command_buf.len);
		parse_ident(ident_buf + 7, &author);
		free(ident_buf);
		read_next_command();
	}
	if (!prefixcmp(command_buf.buf, "committer ")) {
		ident_buf = strbuf_detach(&command_buf, &command_buf.len);
		parse_ident(ident_buf + 10, &committer);
		free(ident_buf);
		read_next_command();
	}
	if (!committer.name.len)
		die("Missing committer line in stream");

	/* Parse the log */
	strbuf_reset(&log);
	term = NULL;
	nbytes = parse_data_len(term);
	parse_data(term, nbytes, &log, NULL);
	read_next_command();

	if (!prefixcmp(command_buf.buf, "from "))
		/* TODO: Support copyfrom */
		read_next_command();
	while (!prefixcmp(command_buf.buf, "merge "))
		/* TODO: Support merges */
		read_next_command();

	/* Translation from Git metadata to SVN metadata */
	build_svn_ident(&svn_ident, &author, &committer);
	populate_revprops(&revprops, &svn_ident, &log);
	dump_export_revision(&revprops);

	do {
		if (!prefixcmp(command_buf.buf, "M "))
			file_change_m(command_buf.buf + 2);
		else if (!prefixcmp(command_buf.buf, "D "))
			file_change_d(command_buf.buf + 2);
		else if (!prefixcmp(command_buf.buf, "R "))
			file_change_cr(command_buf.buf + 2, 1);
		else if (!prefixcmp(command_buf.buf, "C "))
			file_change_cr(command_buf.buf + 2, 0);
		else if (!prefixcmp(command_buf.buf, "N "))
			parse_ignore_notemodify(command_buf.buf + 2);
		else if (!prefixcmp(command_buf.buf, "ls "))
			goto error; /* TODO */
		else if (!strcmp("deleteall", command_buf.buf))
			goto error; /* TODO */
		else
			/* Unrecognized command is left on command_buf */
			break;
	} while (read_next_command() != EOF);

	/* Eat up optional trailing lf */
	if (!command_buf.len)
		read_next_command();
	return;
error:
	die("Unsupported command: %s", command_buf.buf);
}

static void parse_tag(const char *p)
{
	char *term;
	size_t nbytes;

	if (!p)
		die("Missing name after tag in: %s", command_buf.buf);
	read_next_command();

	if (prefixcmp(command_buf.buf, "from "))
		die("Expected 'from committish', found: %s", command_buf.buf);
	p = command_buf.buf + 5;
	if (!p)
		die("Missing committish after from in: %s", command_buf.buf);
	read_next_command();

	if (prefixcmp(command_buf.buf, "tagger "))
		die("Expected 'tagger (name?) email when', found: %s", command_buf.buf);
	p = command_buf.buf + 7;
	if (*p != '<')
		p = strchr(p, '<');
	if (!p)
		die("Missing name or email after tagger in: %s", command_buf.buf);
	if (!(p = strchr(p, '>')))
		die("Malformed email in: %s", command_buf.buf);
	if (!(++ p))
		die("Missing when after email in: %s", command_buf.buf);
	read_next_command();

	term = NULL;
	nbytes = parse_data_len(term);
	parse_data(term, nbytes, NULL, NULL);
	read_next_command();
}

void parse_reset_branch(const char *p)
{
	if (!p)
		die("Missing ref after reset in: %s", command_buf.buf);
	read_next_command();

	if (!prefixcmp(command_buf.buf, "from ")) {
		p = command_buf.buf + 5;
		if (!p)
			die("Missing committish after from in: %s", command_buf.buf);
		read_next_command();
	}

	skip_optional_lf();
}

void svnload_read(void)
{
	read_next_command();

	/* Every function in the loop keeps reading until it
	   encounteres EOF or an unrecognized command; the
	   unrecognized command is left on command_buf */
	while (!feof(infile)) {
		if (!strcmp("blob", command_buf.buf))
			die("Non-inlined blobs unsupported");
		else if (!prefixcmp(command_buf.buf, "ls "))
			goto error; /* TODO */
		else if (!prefixcmp(command_buf.buf, "cat-blob "))
			goto error; /* TODO */
		else if (!prefixcmp(command_buf.buf, "commit "))
			parse_commit(command_buf.buf + 7);
		else if (!prefixcmp(command_buf.buf, "tag "))
			/* TODO: No-op */
			parse_tag(command_buf.buf + 4);
		else if (!prefixcmp(command_buf.buf, "reset "))
			/* TODO: No-op */
			parse_reset_branch(command_buf.buf + 6);
		else if (!strcmp(command_buf.buf, "checkpoint")
			|| !prefixcmp(command_buf.buf, "progress ")) {
			/* Ignored */
			read_next_command();
			skip_optional_lf();
		}
		else if (!prefixcmp(command_buf.buf, "feature ")
			|| !prefixcmp(command_buf.buf, "option "))
			/* Ignored */
			read_next_command();
		else
			goto error;
	};
	return;
error:
	die("Unsupported command: %s", command_buf.buf);
}

int svnload_init(const char *filename)
{
	infile = filename ? fopen(filename, "r") : stdin;
	if (!infile)
		die("Cannot open %s: %s", filename, strerror(errno));
	dump_export_init();
	return 0;
}

void svnload_deinit(void)
{
	strbuf_release(&command_buf);
	strbuf_release(&path_d);
}
