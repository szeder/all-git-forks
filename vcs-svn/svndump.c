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

<<<<<<< HEAD
/* Create memory pool for log messages */
obj_pool_gen(log, char, 4096)

static struct line_buffer input = LINE_BUFFER_INIT;

static char *log_copy(uint32_t length, const char *log)
{
	char *buffer;
	log_free(log_pool.size);
	buffer = log_pointer(log_alloc(length));
	strncpy(buffer, log, length);
	return buffer;
}

static struct {
	uint32_t action, propLength, textLength, srcRev, type;
	uint32_t src[REPO_MAX_PATH_DEPTH], dst[REPO_MAX_PATH_DEPTH];
<<<<<<< HEAD
	uint32_t text_delta, prop_delta;
=======
=======
static struct line_buffer input = LINE_BUFFER_INIT;

static struct {
	uint32_t action, propLength, textLength, srcRev, type;
	struct strbuf src, dst;
	uint32_t text_delta, prop_delta;
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
>>>>>>> vcs-svn: pass paths through to fast-import
} node_ctx;

static struct {
	uint32_t revision;
	unsigned long timestamp;
	struct strbuf log, author;
} rev_ctx;

static struct {
	uint32_t version;
	struct strbuf uuid, url;
} dump_ctx;

<<<<<<< HEAD
static struct {
	uint32_t uuid, revision_number, node_path, node_kind, node_action,
		node_copyfrom_path, node_copyfrom_rev, text_content_length,
		prop_content_length, content_length, svn_fs_dump_format_version,
		/* version 3 format */
		text_delta, prop_delta;
} keys;

=======
>>>>>>> 66a9029... vcs-svn: implement perfect hash for top-level keys
static void reset_node_ctx(char *fname)
{
	node_ctx.type = 0;
	node_ctx.action = NODEACT_UNKNOWN;
	node_ctx.propLength = LENGTH_UNKNOWN;
	node_ctx.textLength = LENGTH_UNKNOWN;
	strbuf_reset(&node_ctx.src);
	node_ctx.srcRev = 0;
<<<<<<< HEAD
	pool_tok_seq(REPO_MAX_PATH_DEPTH, node_ctx.dst, "/", fname);
	node_ctx.text_delta = 0;
	node_ctx.prop_delta = 0;
=======
<<<<<<< HEAD
	node_ctx.srcMode = 0;
	pool_tok_seq(REPO_MAX_PATH_DEPTH, node_ctx.dst, "/", fname);
	node_ctx.mark = 0;
=======
	strbuf_reset(&node_ctx.dst);
	if (fname)
		strbuf_addstr(&node_ctx.dst, fname);
	node_ctx.text_delta = 0;
	node_ctx.prop_delta = 0;
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
>>>>>>> vcs-svn: pass paths through to fast-import
}

static void reset_rev_ctx(uint32_t revision)
{
	rev_ctx.revision = revision;
	rev_ctx.timestamp = 0;
	strbuf_reset(&rev_ctx.log);
	strbuf_reset(&rev_ctx.author);
}

static void reset_dump_ctx(const char *url)
{
	strbuf_reset(&dump_ctx.url);
	if (url)
		strbuf_addstr(&dump_ctx.url, url);
	dump_ctx.version = 1;
	strbuf_reset(&dump_ctx.uuid);
}

<<<<<<< HEAD
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
<<<<<<< HEAD
=======
<<<<<<< HEAD
=======
>>>>>>> vcs-svn: pass paths through to fast-import
	/* version 3 format (Subversion 1.1.0) */
	keys.text_delta = pool_intern("Text-delta");
	keys.prop_delta = pool_intern("Prop-delta");
}

<<<<<<< HEAD
<<<<<<< HEAD
<<<<<<< HEAD
static void handle_property(uint32_t key, const char *val, uint32_t len,
				uint32_t *type_set)
=======
static void handle_property(uint32_t key, const char *val, uint32_t len)
>>>>>>> vcs-svn: pass paths through to fast-import
=======
=======
=======
>>>>>>> 66a9029... vcs-svn: implement perfect hash for top-level keys
>>>>>>> vcs-svn: implement perfect hash for top-level keys
static void handle_property(char *key, const char *val, uint32_t len)
>>>>>>> vcs-svn: implement perfect hash for node-prop keys
{
	switch (strlen(key)) {
	case 7:
		if (memcmp(key, "svn:log", 7))
			break;
		if (!val)
			die("invalid dump: unsets svn:log");
<<<<<<< HEAD
		/* Value length excludes terminating nul. */
<<<<<<< HEAD
		rev_ctx.log = log_copy(len + 1, val);
=======
		strbuf_add(&rev_ctx.log, val, len + 1);
<<<<<<< HEAD
>>>>>>> vcs-svn: pass paths through to fast-import
	} else if (key == keys.svn_author) {
=======
=======
		strbuf_reset(&rev_ctx.log);
		strbuf_add(&rev_ctx.log, val, len);
>>>>>>> vcs-svn: factor out usage of string_pool
		break;
	case 10:
		if (memcmp(key, "svn:author", 10))
			break;
<<<<<<< HEAD
>>>>>>> vcs-svn: implement perfect hash for node-prop keys
		rev_ctx.author = pool_intern(val);
=======
		strbuf_reset(&rev_ctx.author);
		if (val)
			strbuf_add(&rev_ctx.author, val, len);
>>>>>>> vcs-svn: factor out usage of string_pool
		break;
	case 8:
		if (memcmp(key, "svn:date", 8))
			break;
		if (!val)
			die("invalid dump: unsets svn:date");
		if (parse_date_basic(val, &rev_ctx.timestamp, NULL))
			warning("invalid timestamp: %s", val);
<<<<<<< HEAD
<<<<<<< HEAD
	} else if (key == keys.svn_executable || key == keys.svn_special) {
		if (*type_set) {
			if (!val)
				return;
			die("invalid dump: sets type twice");
		}
		if (!val) {
			node_ctx.type = REPO_MODE_BLB;
			return;
		}
		*type_set = 1;
		node_ctx.type = key == keys.svn_executable ?
				REPO_MODE_EXE :
				REPO_MODE_LNK;
	}
=======
	} else if (key == keys.svn_executable) {
=======
		break;
	case 14:
		if (memcmp(key, "svn:executable", 14))
			break;
>>>>>>> vcs-svn: implement perfect hash for node-prop keys
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
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
>>>>>>> vcs-svn: pass paths through to fast-import
}

static void read_props(void)
{
<<<<<<< HEAD
=======
<<<<<<< HEAD
	uint32_t len;
>>>>>>> vcs-svn: implement perfect hash for node-prop keys
	uint32_t key = ~0;
	const char *t;
	/*
	 * NEEDSWORK: to support simple mode changes like
	 *	K 11
	 *	svn:special
	 *	V 1
	 *	*
	 *	D 14
	 *	svn:executable
	 * we keep track of whether a mode has been set and reset to
	 * plain file only if not.  We should be keeping track of the
	 * symlink and executable bits separately instead.
	 */
	uint32_t type_set = 0;
	while ((t = buffer_read_line(&input)) && strcmp(t, "PROPS-END")) {
		uint32_t len;
		const char *val;
		const char type = t[0];

		if (!type || t[1] != ' ')
			die("invalid property line: %s\n", t);
		len = atoi(&t[2]);
		val = buffer_read_string(&input, len);
		buffer_skip_bytes(&input, 1);	/* Discard trailing newline. */

		switch (type) {
		case 'K':
			key = pool_intern(val);
			continue;
		case 'D':
			key = pool_intern(val);
			val = NULL;
			len = 0;
			/* fall through */
		case 'V':
			handle_property(key, val, len, &type_set);
			key = ~0;
<<<<<<< HEAD
			continue;
		default:
			die("invalid property line: %s\n", t);
=======
			buffer_read_line();
=======
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
>>>>>>> 8dfcb73... vcs-svn: implement perfect hash for node-prop keys
>>>>>>> vcs-svn: implement perfect hash for node-prop keys
		}
	}
}

static void handle_node(void)
{
<<<<<<< HEAD
<<<<<<< HEAD
	uint32_t mark = 0;
	const uint32_t type = node_ctx.type;
	const int have_props = node_ctx.propLength != LENGTH_UNKNOWN;
=======
=======
>>>>>>> vcs-svn: eliminate repo_tree structure
<<<<<<< HEAD
	if (node_ctx.propLength != LENGTH_UNKNOWN && node_ctx.propLength)
		read_props();

	if (node_ctx.srcRev)
		node_ctx.srcMode = repo_copy(node_ctx.srcRev, node_ctx.src, node_ctx.dst);

	if (node_ctx.textLength != LENGTH_UNKNOWN &&
	    node_ctx.type != REPO_MODE_DIR)
		node_ctx.mark = next_blob_mark();
>>>>>>> vcs-svn: prepare to eliminate repo_tree structure

	if (node_ctx.text_delta)
		die("text deltas not supported");
	if (node_ctx.textLength != LENGTH_UNKNOWN)
		mark = next_blob_mark();
	if (node_ctx.action == NODEACT_DELETE) {
		if (mark || have_props || node_ctx.srcRev)
			die("invalid dump: deletion node has "
				"copyfrom info, text, or properties");
		return repo_delete(node_ctx.dst);
	}
	if (node_ctx.action == NODEACT_REPLACE) {
		repo_delete(node_ctx.dst);
		node_ctx.action = NODEACT_ADD;
	}
	if (node_ctx.srcRev) {
		repo_copy(node_ctx.srcRev, node_ctx.src, node_ctx.dst);
		if (node_ctx.action == NODEACT_ADD)
			node_ctx.action = NODEACT_CHANGE;
	}
	if (mark && type == REPO_MODE_DIR)
		die("invalid dump: directories cannot have text attached");
	if (node_ctx.action == NODEACT_CHANGE && !~*node_ctx.dst) {
		if (type != REPO_MODE_DIR)
			die("invalid dump: root of tree is not a regular file");
	} else if (node_ctx.action == NODEACT_CHANGE) {
		uint32_t mode = repo_modify_path(node_ctx.dst, 0, mark);
		if (!mode)
			die("invalid dump: path to be modified is missing");
		if (mode == REPO_MODE_DIR && type != REPO_MODE_DIR)
			die("invalid dump: cannot modify a directory into a file");
		if (mode != REPO_MODE_DIR && type == REPO_MODE_DIR)
			die("invalid dump: cannot modify a file into a directory");
		node_ctx.type = mode;
	} else if (node_ctx.action == NODEACT_ADD) {
		if (!mark && type != REPO_MODE_DIR)
			die("invalid dump: adds node without text");
		repo_add(node_ctx.dst, type, mark);
	} else {
		die("invalid dump: Node-path block lacks Node-action");
	}
<<<<<<< HEAD
	if (have_props) {
		const uint32_t old_mode = node_ctx.type;
		if (!node_ctx.prop_delta)
			node_ctx.type = type;
		if (node_ctx.propLength)
			read_props();
		if (node_ctx.type != old_mode)
			repo_modify_path(node_ctx.dst, node_ctx.type, mark);
	}
	if (mark)
		fast_export_blob(node_ctx.type, mark,
				 node_ctx.textLength, &input);
=======
<<<<<<< HEAD

	if (node_ctx.propLength == LENGTH_UNKNOWN && node_ctx.srcMode)
		node_ctx.type = node_ctx.srcMode;

	if (node_ctx.mark)
		fast_export_blob(node_ctx.type, node_ctx.mark, node_ctx.textLength);
	else if (node_ctx.textLength != LENGTH_UNKNOWN)
		buffer_skip_bytes(node_ctx.textLength);
=======
	if (!mark)
=======
	uint32_t mark = 0, old_mode, old_mark;
=======
	uint32_t old_mode;
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
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
<<<<<<< HEAD
	repo_add(node_ctx.dst, node_ctx.type, mark);
	if (!have_text)
>>>>>>> 566fd14... vcs-svn: prepare to eliminate repo_tree structure
=======
	if (type == REPO_MODE_DIR)	/* directories are not tracked. */
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
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
<<<<<<< HEAD
<<<<<<< HEAD
	fast_export_blob_delta(node_ctx.type, mark, old_mode, old_mark,
				node_ctx.textLength, &input);
>>>>>>> ae828d6... vcs-svn: do not rely on marks for old blobs
<<<<<<< HEAD
>>>>>>> vcs-svn: do not rely on marks for old blobs
=======
=======
	fast_export_delta(node_ctx.type, REPO_MAX_PATH_DEPTH, node_ctx.dst,
=======
	fast_export_delta(node_ctx.type, node_ctx.dst.buf,
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
				old_mode, old_data, node_ctx.textLength, &input);
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
>>>>>>> vcs-svn: eliminate repo_tree structure
}

static void begin_revision(void)
{
	if (!rev_ctx.revision)	/* revision 0 gets no git commit. */
		return;
	fast_export_begin_commit(rev_ctx.revision, rev_ctx.author.buf,
		rev_ctx.log.buf, dump_ctx.uuid.buf, dump_ctx.url.buf,
		rev_ctx.timestamp);
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

<<<<<<< HEAD
	reset_dump_ctx(pool_intern(url));
<<<<<<< HEAD
	while ((t = buffer_read_line(&input))) {
=======
	while ((t = buffer_read_line())) {
=======
	reset_dump_ctx(url);
	while ((t = buffer_read_line(&input))) {
>>>>>>> 4f1c5cb... vcs-svn: factor out usage of string_pool
>>>>>>> vcs-svn: factor out usage of string_pool
		val = strstr(t, ": ");
		if (!val)
			continue;
		*val++ = '\0';
		*val++ = '\0';

		/* strlen(key) */
		switch (val - t - 2) { 
		case 26:
			if (memcmp(t, "SVN-fs-dump-format-version", 26))
				continue;
			dump_ctx.version = atoi(val);
			if (dump_ctx.version > 3)
				die("expected svn dump format version <= 3, found %"PRIu32,
				    dump_ctx.version);
			break;
		case 4:
			if (memcmp(t, "UUID", 4))
				continue;
			strbuf_reset(&dump_ctx.uuid);
			strbuf_addstr(&dump_ctx.uuid, val);
			break;
		case 15:
			if (memcmp(t, "Revision-number", 15))
				continue;
			if (active_ctx == NODE_CTX)
				handle_node();
			if (active_ctx == REV_CTX)
				begin_revision();
			if (active_ctx != DUMP_CTX)
				end_revision();
			active_ctx = REV_CTX;
			reset_rev_ctx(atoi(val));
			break;
		case 9:
			if (prefixcmp(t, "Node-"))
				continue;
			if (!memcmp(t + strlen("Node-"), "path", 4)) {
				if (active_ctx == NODE_CTX)
					handle_node();
				if (active_ctx == REV_CTX)
					begin_revision();
				active_ctx = NODE_CTX;
				reset_node_ctx(val);
				break;
			}
			if (memcmp(t + strlen("Node-"), "kind", 4))
				continue;
			if (!strcmp(val, "dir"))
				node_ctx.type = REPO_MODE_DIR;
			else if (!strcmp(val, "file"))
				node_ctx.type = REPO_MODE_BLB;
			else
				fprintf(stderr, "Unknown node-kind: %s\n", val);
			break;
		case 11:
			if (memcmp(t, "Node-action", 11))
				continue;
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
			break;
		case 18:
			if (memcmp(t, "Node-copyfrom-path", 18))
				continue;
			strbuf_reset(&node_ctx.src);
			strbuf_addstr(&node_ctx.src, val);
			break;
		case 17:
			if (memcmp(t, "Node-copyfrom-rev", 17))
				continue;
			node_ctx.srcRev = atoi(val);
			break;
		case 19:
			if (!memcmp(t, "Text-content-length", 19)) {
				node_ctx.textLength = atoi(val);
				break;
			}
			if (memcmp(t, "Prop-content-length", 19))
				continue;
			node_ctx.propLength = atoi(val);
<<<<<<< HEAD
		} else if (key == keys.text_delta) {
			node_ctx.text_delta = !strcmp(val, "true");
		} else if (key == keys.prop_delta) {
			node_ctx.prop_delta = !strcmp(val, "true");
=======
<<<<<<< HEAD
>>>>>>> vcs-svn: implement perfect hash for top-level keys
		} else if (key == keys.content_length) {
=======
			break;
		case 10:
			if (!memcmp(t, "Text-delta", 10)) {
				node_ctx.text_delta = !strcmp(val, "true");
				break;
			}
			if (memcmp(t, "Prop-delta", 10))
				continue;
			node_ctx.prop_delta = !strcmp(val, "true");
			break;
		case 14:
			if (memcmp(t, "Content-length", 14))
				continue;
>>>>>>> 66a9029... vcs-svn: implement perfect hash for top-level keys
			len = atoi(val);
			buffer_read_line(&input);
			if (active_ctx == REV_CTX) {
				read_props();
			} else if (active_ctx == NODE_CTX) {
				handle_node();
				active_ctx = INTERNODE_CTX;
			} else {
				fprintf(stderr, "Unexpected content length header: %"PRIu32"\n", len);
				buffer_skip_bytes(&input, len);
			}
		}
	}
	if (active_ctx == NODE_CTX)
		handle_node();
	if (active_ctx == REV_CTX)
		begin_revision();
	if (active_ctx != DUMP_CTX)
		end_revision();
}

int svndump_init(const char *filename)
{
<<<<<<< HEAD
	if (buffer_init(&input, filename))
		return error("cannot open %s: %s", filename, strerror(errno));
=======
<<<<<<< HEAD
	buffer_init(filename);
>>>>>>> vcs-svn: eliminate repo_tree structure
	repo_init();
=======
	if (buffer_init(&input, filename))
		return error("cannot open %s: %s", filename, strerror(errno));
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
	fast_export_init(REPORT_FILENO);
	strbuf_init(&dump_ctx.uuid, 4096);
	strbuf_init(&dump_ctx.url, 4096);
	strbuf_init(&rev_ctx.log, 4096);
	strbuf_init(&rev_ctx.author, 4096);
	strbuf_init(&node_ctx.src, 4096);
	strbuf_init(&node_ctx.dst, 4096);
	reset_dump_ctx(NULL);
	reset_rev_ctx(0);
	reset_node_ctx(NULL);
<<<<<<< HEAD
	init_keys();
<<<<<<< HEAD
	return 0;
=======
=======
	return 0;
>>>>>>> 66a9029... vcs-svn: implement perfect hash for top-level keys
>>>>>>> vcs-svn: implement perfect hash for top-level keys
}

void svndump_deinit(void)
{
	fast_export_deinit();
	reset_dump_ctx(NULL);
	reset_rev_ctx(0);
	reset_node_ctx(NULL);
<<<<<<< HEAD
	if (buffer_deinit(&input))
=======
<<<<<<< HEAD
	if (buffer_deinit())
=======
	strbuf_release(&rev_ctx.log);
	strbuf_release(&node_ctx.src);
	strbuf_release(&node_ctx.dst);
	if (buffer_deinit(&input))
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
>>>>>>> vcs-svn: pass paths through to fast-import
		fprintf(stderr, "Input error\n");
	if (ferror(stdout))
		fprintf(stderr, "Output error\n");
}

void svndump_reset(void)
{
<<<<<<< HEAD
	log_reset();
<<<<<<< HEAD
	buffer_reset(&input);
=======
<<<<<<< HEAD
	buffer_reset();
=======
	fast_export_reset();
	buffer_reset(&input);
<<<<<<< HEAD
>>>>>>> 8ab8687... vcs-svn: explicitly close streams used for delta application at exit
>>>>>>> vcs-svn: explicitly close streams used for delta application at exit
	repo_reset();
=======
>>>>>>> 7e69325... vcs-svn: eliminate repo_tree structure
	reset_dump_ctx(~0);
	reset_rev_ctx(0);
	reset_node_ctx(NULL);
=======
	fast_export_reset();
	buffer_reset(&input);
<<<<<<< HEAD
	pool_reset();
>>>>>>> 01823f6... vcs-svn: pass paths through to fast-import
=======
>>>>>>> 4f1c5cb... vcs-svn: factor out usage of string_pool
}
