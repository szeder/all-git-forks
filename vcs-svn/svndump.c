/*
 * Parse and rearrange a svnadmin dump.
 * Create the dump with:
 * svnadmin dump --incremental -r<startrev>:<endrev> <repository> >outfile
 *
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "cache.h"
#include "repo_tree.h"
#include "fast_export.h"
#include "line_buffer.h"
#include "string_pool.h"
#include "strbuf.h"

#define REPORT_FILENO 3

#define NODEACT_REPLACE 4
#define NODEACT_DELETE 3
#define NODEACT_ADD 2
#define NODEACT_CHANGE 1
#define NODEACT_UNKNOWN 0

/* States: */
#define DUMP_CTX 0	/* dump metadata */
#define REV_CTX  1	/* revision metadata */
#define NODE_CTX 2	/* node metadata */
#define INTERNODE_CTX 3	/* between nodes */

#define LENGTH_UNKNOWN (~0)
#define DATE_RFC2822_LEN 31

static struct line_buffer input = LINE_BUFFER_INIT;

static struct {
	uint32_t action, propLength, textLength, srcRev, type;
	struct strbuf src, dst;
	uint32_t text_delta, prop_delta;
} node_ctx;

static struct {
	uint32_t revision, author;
	unsigned long timestamp;
	struct strbuf log;
} rev_ctx;

static struct {
	uint32_t version, uuid, url;
} dump_ctx;

static struct {
	uint32_t uuid, revision_number, node_path, node_kind, node_action,
		node_copyfrom_path, node_copyfrom_rev, text_content_length,
		prop_content_length, content_length, svn_fs_dump_format_version,
		/* version 3 format */
		text_delta, prop_delta;
} keys;

static void reset_node_ctx(char *fname)
{
	node_ctx.type = 0;
	node_ctx.action = NODEACT_UNKNOWN;
	node_ctx.propLength = LENGTH_UNKNOWN;
	node_ctx.textLength = LENGTH_UNKNOWN;
	strbuf_reset(&node_ctx.src);
	node_ctx.srcRev = 0;
	strbuf_reset(&node_ctx.dst);
	if (fname)
		strbuf_addstr(&node_ctx.dst, fname);
	node_ctx.text_delta = 0;
	node_ctx.prop_delta = 0;
}

static void reset_rev_ctx(uint32_t revision)
{
	rev_ctx.revision = revision;
	rev_ctx.timestamp = 0;
	strbuf_reset(&rev_ctx.log);
	rev_ctx.author = ~0;
}

static void reset_dump_ctx(uint32_t url)
{
	dump_ctx.url = url;
	dump_ctx.version = 1;
	dump_ctx.uuid = ~0;
}

static void init_keys(void)
{
	keys.uuid = pool_intern("UUID");
	keys.revision_number = pool_intern("Revision-number");
	keys.node_path = pool_intern("Node-path");
	keys.node_kind = pool_intern("Node-kind");
	keys.node_action = pool_intern("Node-action");
	keys.node_copyfrom_path = pool_intern("Node-copyfrom-path");
	keys.node_copyfrom_rev = pool_intern("Node-copyfrom-rev");
	keys.text_content_length = pool_intern("Text-content-length");
	keys.prop_content_length = pool_intern("Prop-content-length");
	keys.content_length = pool_intern("Content-length");
	keys.svn_fs_dump_format_version = pool_intern("SVN-fs-dump-format-version");
	/* version 3 format (Subversion 1.1.0) */
	keys.text_delta = pool_intern("Text-delta");
	keys.prop_delta = pool_intern("Prop-delta");
}

static void handle_property(char *key, const char *val, uint32_t len)
{
	switch (strlen(key)) {
	case 7:
		if (memcmp(key, "svn:log", 7))
			break;
		if (!val)
			die("invalid dump: unsets svn:log");
		/* Value length excludes terminating nul. */
		strbuf_add(&rev_ctx.log, val, len + 1);
		break;
	case 10:
		if (memcmp(key, "svn:author", 10))
			break;
		rev_ctx.author = pool_intern(val);
		break;
	case 8:
		if (memcmp(key, "svn:date", 8))
			break;
		if (!val)
			die("invalid dump: unsets svn:date");
		if (parse_date_basic(val, &rev_ctx.timestamp, NULL))
			warning("invalid timestamp: %s", val);
		break;
	case 14:
		if (memcmp(key, "svn:executable", 14))
			break;
		if (val)
			node_ctx.type = REPO_MODE_EXE;
		else if (node_ctx.type == REPO_MODE_EXE)
			node_ctx.type = REPO_MODE_BLB;
		break;
	case 11:
		if (memcmp(key, "svn:special", 11))
			break;
		if (val)
			node_ctx.type = REPO_MODE_LNK;
		else if (node_ctx.type == REPO_MODE_LNK)
			node_ctx.type = REPO_MODE_BLB;
		break;
	}
}

static void die_short_read(struct line_buffer *input)
{
	if (buffer_ferror(input))
		die_errno("error reading dump file");
	die("invalid dump: unexpected end of file");
}

static void read_props(void)
{
	char key[16] = {0};
	for (;;) {
		char *t = buffer_read_line(&input);
		uint32_t len;
		const char *val;
		char type;

		if (!t)
			die_short_read(&input);
		if (!strcmp(t, "PROPS-END"))
			return;

		type = t[0];
		if (!type || t[1] != ' ')
			die("invalid property line: %s\n", t);
		len = atoi(&t[2]);
		val = buffer_read_string(&input, len);
		if (!val)
			die_short_read(&input);
		if (buffer_read_char(&input) != '\n')
			die("invalid dump: incorrect key length");

		switch (type) {
		case 'K':
		case 'D':
			if (len < sizeof(key))
				memcpy(key, val, len + 1);
			else	/* nonstandard key. */
				*key = '\0';
			if (type == 'K')
				continue;
			assert(type == 'D');
			val = NULL;
			len = 0;
			/* fall through */
		case 'V':
			handle_property(key, val, len);
			*key = '\0';
			continue;
		default:
			die("invalid property line: %s\n", t);
		}
	}
}

static void handle_node(void)
{
	uint32_t old_mode;
	const uint32_t type = node_ctx.type;
	const int have_props = node_ctx.propLength != LENGTH_UNKNOWN;
	const int have_text = node_ctx.textLength != LENGTH_UNKNOWN;
	/*
	 * Old text for this node (preimage for delta):
	 *  NULL	- directory or bug
	 *  empty_blob	- empty
	 *  "<dataref>"	- data to be retrieved from fast-import
	 */
	static const char *const empty_blob = "::empty::";
	const char *old_data = NULL;

	if (node_ctx.action == NODEACT_DELETE) {
		if (have_text || have_props || node_ctx.srcRev)
			die("invalid dump: deletion node has "
				"copyfrom info, text, or properties");
		return repo_delete(node_ctx.dst.buf);
	}
	if (node_ctx.action == NODEACT_REPLACE) {
		repo_delete(node_ctx.dst.buf);
		node_ctx.action = NODEACT_ADD;
	}
	if (node_ctx.srcRev) {
		repo_copy(node_ctx.srcRev, node_ctx.src.buf, node_ctx.dst.buf);
		if (node_ctx.action == NODEACT_ADD)
			node_ctx.action = NODEACT_CHANGE;
	}
	if (have_text && type == REPO_MODE_DIR)
		die("invalid dump: directories cannot have text attached");

	/*
	 * Find old content (old_data) and decide on the new mode.
	 */
	if (node_ctx.action == NODEACT_CHANGE && !*node_ctx.dst.buf) {
		if (type != REPO_MODE_DIR)
			die("invalid dump: root of tree is not a regular file");
		old_data = NULL;
	} else if (node_ctx.action == NODEACT_CHANGE) {
		uint32_t mode;
		old_data = repo_read_path(node_ctx.dst.buf, &mode);
		if (mode == REPO_MODE_DIR && type != REPO_MODE_DIR)
			die("invalid dump: cannot modify a directory into a file");
		if (mode != REPO_MODE_DIR && type == REPO_MODE_DIR)
			die("invalid dump: cannot modify a file into a directory");
		node_ctx.type = mode;
	} else if (node_ctx.action == NODEACT_ADD) {
		if (type == REPO_MODE_DIR)
			old_data = NULL;
		else if (have_text)
			old_data = empty_blob;
		else
			die("invalid dump: adds node without text");
	} else {
		die("invalid dump: Node-path block lacks Node-action");
	}

	/*
	 * Adjust mode to reflect properties.
	 */
	old_mode = node_ctx.type;
	if (have_props) {
		if (!node_ctx.prop_delta)
			node_ctx.type = type;
		if (node_ctx.propLength)
			read_props();
	}

	/*
	 * Save the result.
	 */
	if (type == REPO_MODE_DIR)	/* directories are not tracked. */
		return;
	assert(old_data);
	if (old_data == empty_blob)
		/* For the fast_export_* functions, NULL means empty. */
		old_data = NULL;
	if (!have_text) {
		fast_export_modify(node_ctx.dst.buf, node_ctx.type, old_data);
		return;
	}
	if (!node_ctx.text_delta) {
		fast_export_modify(node_ctx.dst.buf, node_ctx.type, "inline");
		fast_export_data(node_ctx.type, node_ctx.textLength, &input);
		return;
	}
	fast_export_delta(node_ctx.type, node_ctx.dst.buf,
				old_mode, old_data, node_ctx.textLength, &input);
}

static void begin_revision(void)
{
	if (!rev_ctx.revision)	/* revision 0 gets no git commit. */
		return;
	fast_export_begin_commit(rev_ctx.revision, rev_ctx.author, rev_ctx.log.buf,
		dump_ctx.uuid, dump_ctx.url, rev_ctx.timestamp);
}

static void end_revision(void)
{
	if (rev_ctx.revision)
		fast_export_end_commit(rev_ctx.revision);
}

void svndump_read(const char *url)
{
	char *val;
	char *t;
	uint32_t active_ctx = DUMP_CTX;
	uint32_t len;
	uint32_t key;

	reset_dump_ctx(pool_intern(url));
	while ((t = buffer_read_line(&input))) {
		val = strstr(t, ": ");
		if (!val)
			continue;
		*val++ = '\0';
		*val++ = '\0';
		key = pool_intern(t);

		if (key == keys.svn_fs_dump_format_version) {
			dump_ctx.version = atoi(val);
			if (dump_ctx.version > 3)
				die("expected svn dump format version <= 3, found %d",
				    dump_ctx.version);
		} else if (key == keys.uuid) {
			dump_ctx.uuid = pool_intern(val);
		} else if (key == keys.revision_number) {
			if (active_ctx == NODE_CTX)
				handle_node();
			if (active_ctx == REV_CTX)
				begin_revision();
			if (active_ctx != DUMP_CTX)
				end_revision();
			active_ctx = REV_CTX;
			reset_rev_ctx(atoi(val));
		} else if (key == keys.node_path) {
			if (active_ctx == NODE_CTX)
				handle_node();
			if (active_ctx == REV_CTX)
				begin_revision();
			active_ctx = NODE_CTX;
			reset_node_ctx(val);
		} else if (key == keys.node_kind) {
			if (!strcmp(val, "dir"))
				node_ctx.type = REPO_MODE_DIR;
			else if (!strcmp(val, "file"))
				node_ctx.type = REPO_MODE_BLB;
			else
				fprintf(stderr, "Unknown node-kind: %s\n", val);
		} else if (key == keys.node_action) {
			if (!strcmp(val, "delete")) {
				node_ctx.action = NODEACT_DELETE;
			} else if (!strcmp(val, "add")) {
				node_ctx.action = NODEACT_ADD;
			} else if (!strcmp(val, "change")) {
				node_ctx.action = NODEACT_CHANGE;
			} else if (!strcmp(val, "replace")) {
				node_ctx.action = NODEACT_REPLACE;
			} else {
				fprintf(stderr, "Unknown node-action: %s\n", val);
				node_ctx.action = NODEACT_UNKNOWN;
			}
		} else if (key == keys.node_copyfrom_path) {
			strbuf_reset(&node_ctx.src);
			strbuf_addstr(&node_ctx.src, val);
		} else if (key == keys.node_copyfrom_rev) {
			node_ctx.srcRev = atoi(val);
		} else if (key == keys.text_content_length) {
			node_ctx.textLength = atoi(val);
		} else if (key == keys.prop_content_length) {
			node_ctx.propLength = atoi(val);
		} else if (key == keys.text_delta) {
			node_ctx.text_delta = !strcmp(val, "true");
		} else if (key == keys.prop_delta) {
			node_ctx.prop_delta = !strcmp(val, "true");
		} else if (key == keys.content_length) {
			len = atoi(val);
			t = buffer_read_line(&input);
			if (!t)
				die_short_read(&input);
			if (*t)
				die("invalid dump: expected blank line after content length header");
			if (active_ctx == REV_CTX) {
				read_props();
			} else if (active_ctx == NODE_CTX) {
				handle_node();
				active_ctx = INTERNODE_CTX;
			} else {
				fprintf(stderr, "Unexpected content length header: %d\n", len);
				if (buffer_skip_bytes(&input, len) != len)
					die_short_read(&input);
			}
		}
	}
	if (buffer_ferror(&input))
		die_short_read(&input);
	if (active_ctx == NODE_CTX)
		handle_node();
	if (active_ctx == REV_CTX)
		begin_revision();
	if (active_ctx != DUMP_CTX)
		end_revision();
}

int svndump_init(const char *filename)
{
	if (buffer_init(&input, filename))
		return error("cannot open %s: %s", filename, strerror(errno));
	fast_export_init(REPORT_FILENO);
	strbuf_init(&rev_ctx.log, 4096);
	strbuf_init(&node_ctx.src, 4096);
	strbuf_init(&node_ctx.dst, 4096);
	reset_dump_ctx(~0);
	reset_rev_ctx(0);
	reset_node_ctx(NULL);
	init_keys();
	return 0;
}

void svndump_deinit(void)
{
	fast_export_deinit();
	reset_dump_ctx(~0);
	reset_rev_ctx(0);
	reset_node_ctx(NULL);
	strbuf_release(&rev_ctx.log);
	strbuf_release(&node_ctx.src);
	strbuf_release(&node_ctx.dst);
	if (buffer_deinit(&input))
		fprintf(stderr, "Input error\n");
	if (ferror(stdout))
		fprintf(stderr, "Output error\n");
}

void svndump_reset(void)
{
	fast_export_reset();
	buffer_reset(&input);
	pool_reset();
}
