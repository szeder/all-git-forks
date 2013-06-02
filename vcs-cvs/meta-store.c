#include "vcs-cvs/meta-store.h"
#include "cache.h"
#include "notes.h"
#include "builtin.h"
#include "blob.h"
#include "refs.h"

static const char *ref_prefix = NULL;
static const char *private_ref_prefix = NULL;
static const char *private_tags_ref_prefix = NULL;

void set_ref_prefix_remote(const char *remote_name)
{
	struct strbuf ref_prefix_sb = STRBUF_INIT;
	struct strbuf private_ref_prefix_sb = STRBUF_INIT;
	struct strbuf private_tags_ref_prefix_sb = STRBUF_INIT;

	if (is_bare_repository()) {
		strbuf_addstr(&ref_prefix_sb, "refs/heads/");
		strbuf_addstr(&private_ref_prefix_sb, "refs/cvsimport/heads/");
		strbuf_addstr(&private_tags_ref_prefix_sb, "refs/cvsimport/tags/");
	}
	else {
		strbuf_addf(&ref_prefix_sb, "refs/remotes/%s/", remote_name);
		strbuf_addf(&private_ref_prefix_sb, "refs/cvsimport/remotes/%s/heads/", remote_name);
		strbuf_addf(&private_tags_ref_prefix_sb, "refs/cvsimport/remotes/%s/tags/", remote_name);
	}

	ref_prefix = strbuf_detach(&ref_prefix_sb, NULL);
	private_ref_prefix = strbuf_detach(&private_ref_prefix_sb, NULL);
	private_tags_ref_prefix = strbuf_detach(&private_tags_ref_prefix_sb, NULL);
}

const char *get_meta_ref_prefix()
{
	return "refs/cvsmeta/heads/";
}

const char *get_meta_tags_ref_prefix()
{
	return "refs/cvsmeta/tags/";
}

const char *get_ref_prefix()
{
	return ref_prefix;
}

const char *get_private_ref_prefix()
{
	return private_ref_prefix;
}

const char *get_private_tags_ref_prefix()
{
	return private_tags_ref_prefix;
}

/*
 * metadata work
 */

char *read_note_of(unsigned char sha1[20], const char *notes_ref, unsigned long *size)
{
	struct notes_tree *t;
	const unsigned char *note;
	enum object_type type;
	char *buf = NULL;

	t = xcalloc(1, sizeof(*t));
	init_notes(t, notes_ref, combine_notes_overwrite, 0);
	note = get_note(t, sha1);
	if (note) {
		//fprintf(stderr, "note %s:\n", sha1_to_hex(note));
		buf = read_sha1_file(note, &type, size);
		if (!buf)
			die("Cannot read sha1 %s", sha1_to_hex(note));
	}

	free_notes(t);
	free(t);

	return buf;
}

void add_cvs_revision_hash(struct hash_table *meta_hash,
		       const char *path,
		       const char *revision,
		       time_t timestamp,
		       int isdead,
		       int isexec,
		       int mark)
{
	void **pos;
	unsigned int hash;
	struct cvs_revision *rev_meta;

	rev_meta = xcalloc(1, sizeof(*rev_meta));
	rev_meta->path = xstrdup(path);
	rev_meta->revision = xstrdup(revision);
	rev_meta->timestamp = timestamp;
	rev_meta->ismeta = 1;
	rev_meta->isdead = isdead;
	rev_meta->isexec = isexec;
	rev_meta->mark = mark;

	hash = hash_path(path);
	pos = insert_hash(hash, rev_meta, meta_hash);
	if (pos) {
		die("add_cvs_revision collision");
		*pos = rev_meta;
	}
}

/*static void meta_line_add_attr(struct strbuf *line, const char *attr, const char *value, int *want_comma)
{
	if (*want_comma)
		strbuf_addch(line, ',');
	*want_comma = 1;
	strbuf_addf(line, "%s=%s", attr, value);
}

const char *attributes[] = {
	"synctime",
	"revision",
	"isdead",
	"ispushed"
};

enum {
	ATTR_SYNCTIME,
	ATTR_REVISION,
	ATTR_ISDEAD,
};*/

void format_add_meta_line(struct strbuf *sb, struct cvs_revision *rev)
{
	//int want_comma = 0;

	/*meta_line_add_attr(line, attributes[ATTR_REVISION], rev->revision, &want_comma);

	if (rev->isdead)
		meta_line_add_attr(line, attributes[ATTR_ISDEAD], "y", &want_comma);*/

	strbuf_addf(sb, "%s:%s:%s\n", rev->revision, rev->isdead ? "dead" : "", rev->path);
}

/*char *parse_meta_line(char *buf, unsigned long len, char **first, char **second, char *p)
{
	char *start = p;
	*first = NULL;
	*second = NULL;
	while (p < buf + len) {
		if (*p == '\n') {
			*p = 0;
			if (!*first)
				*first = start;
			else
				*second = start;
			start = ++p;
			return p;
		}
		else if (!*first && *p == ':') {
			*p = 0;
			*first = start;
			start = ++p;
			continue;
		}
		++p;
	}
	return NULL;
}*/

char *parse_meta_line(char *buf, unsigned long len, char **first, char **second, char **attr, char *p)
{
	char *start = p;
	char *sep;
	*first = NULL;
	*second = NULL;
	*attr = NULL;

	if (p >= buf + len)
		return NULL;

	p = memchr(p, '\n', p - (buf + len));
	if (!p) // every meta line should end with '\n'
		return NULL;
	*p++ = 0;
	*first = start;

	sep = strchr(start, ':');
	if (!sep)
		return p;
	*sep++ = '\0';
	*second = sep;

	sep = strchr(sep, ':');
	if (!sep)
		return p;
	*sep++ = '\0';
	*attr = *second;
	*second = sep;

	return p;
}

int has_revision_meta(unsigned char *sha1, const char *notes_ref)
{
	struct notes_tree *t;
	const unsigned char *note;

	t = xcalloc(1, sizeof(*t));
	init_notes(t, notes_ref, combine_notes_overwrite, 0);
	note = get_note(t, sha1);

	free_notes(t);
	free(t);
	return !!note;
}

int load_revision_meta(unsigned char *sha1, const char *notes_ref, time_t *timestamp, struct hash_table **revision_meta_hash)
{
	char *buf;
	char *p;
	char *first;
	char *second;
	char *attr;
	unsigned long size;
	*revision_meta_hash = NULL;

	buf = read_note_of(sha1, notes_ref, &size);
	if (!buf)
		return 0;

	*revision_meta_hash = xmalloc(sizeof(struct hash_table));
	init_hash(*revision_meta_hash);

	p = buf;
	while ((p = parse_meta_line(buf, size, &first, &second, &attr, p))) {
		if (strcmp(first, "--") == 0)
			break;
		fprintf(stderr, "option: %s=>%s\n", first, second);
		if (!strcmp(first, "UPDATE") && timestamp) {
			*timestamp = atol(second);
			if (*timestamp == 0)
				die("cvs metadata next UPDATE time is wrong");
		}
	}

	while ((p = parse_meta_line(buf, size, &first, &second, &attr, p))) {
		int isdead;
		if (!second || !attr)
			die("malformed metadata: %s:%s:%s", first, attr, second);
		isdead = !!strstr(attr, "dead");
		add_cvs_revision_hash(*revision_meta_hash, second, first, 0, isdead, 0, 0);
	}

	free(buf);
	return 0;
}

static void commit_meta(struct notes_tree *t, const char *notes_ref, const char *commit_msg)
{
	struct strbuf commit_msg_sb = STRBUF_INIT;
	unsigned char tree_sha1[20];
	unsigned char parent_sha1[20];
	unsigned char result_sha1[20];
	struct commit_list *parents = NULL;

	if (write_notes_tree(t, tree_sha1) != 0)
		die("write_notes_tree failed");

	if (!read_ref(t->ref, parent_sha1)) {
		struct commit *parent = lookup_commit(parent_sha1);
		if (!parent || parse_commit(parent))
			die("Failed to find/parse commit %s", t->ref);
		commit_list_insert(parent, &parents);
	}

	strbuf_addstr(&commit_msg_sb, commit_msg);
	if (commit_tree(&commit_msg_sb, tree_sha1, parents, result_sha1, NULL, NULL))
		die("Failed to commit notes tree to database");
	strbuf_release(&commit_msg_sb);

	/*
	 * reflog
	 */
	update_ref("cvs metadate update", notes_ref, result_sha1, NULL, 0, DIE_ON_ERR);
}

static int save_note_for(const unsigned char *commit_sha1, const char *notes_ref,
			const char *commit_msg, const char *note)
{
	struct notes_tree *t;
	unsigned char note_sha1[20];

	t = xcalloc(1, sizeof(*t));
	init_notes(t, notes_ref, combine_notes_overwrite, 0);

	if (write_sha1_file(note, strlen(note), blob_type, note_sha1))
		error(_("unable to write note object"));

	if (add_note(t, commit_sha1, note_sha1, combine_notes_overwrite) != 0)
		die("add_note failed");

	commit_meta(t, notes_ref, commit_msg);

	free_notes(t);
	free(t);

	return 0;
}

static int save_revision_meta_cb(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;
	struct strbuf *sb = data;

	format_add_meta_line(sb, rev);
	return 0;
}

int save_revision_meta(unsigned char *sha1, const char *notes_ref, const char *msg, struct hash_table *revision_meta_hash)
{
	struct strbuf sb;
	strbuf_init(&sb, revision_meta_hash->nr * 64);
	//strbuf_addf(&sb, "UPDATE:%ld\n", meta->last_revision_timestamp);
	strbuf_addstr(&sb, "--\n");

	for_each_hash(revision_meta_hash, save_revision_meta_cb, &sb);

	save_note_for(sha1, notes_ref, msg, sb.buf);
	strbuf_release(&sb);
	return 0;
}
