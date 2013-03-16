#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "builtin.h"
#include "notes.h"
#include "blob.h"
#include "refs.h"

#include "vcs-cvs/meta.h"

long ps = 0;
static const char *ref_prefix = NULL;

const char *get_meta_ref_prefix()
{
	return "refs/meta/";
}

void set_ref_prefix(const char *prefix)
{
	ref_prefix = xstrdup(prefix);
}

const char *get_ref_prefix()
{
	return ref_prefix;
}

static inline unsigned char icase_hash(unsigned char c)
{
	return c & ~((c & 0x40) >> 1);
}

//static unsigned int hash_path(const char *path)
unsigned int hash_path(const char *path)
{
	//unsigned int hash = 0x123;
	unsigned int hash = 0x12375903;

	while (*path) {
		unsigned char c = *path++;
		//c = icase_hash(c);
		hash = hash*101 + c;
	}
	return hash;
}

static unsigned int hash_author_msg(const char *author, const char *msg)
{
	unsigned int hash = 0x123;

	while(*author) {
		unsigned char c = *author++;
		c = icase_hash(c);
		hash = hash*101 + c;
	}

	while(*msg) {
		//if (*msg == ' ' || *msg == '\t') {
		if (isspace(*msg)) {
			msg++;
			continue;
		}
		unsigned char c = *msg++;
		c = icase_hash(c);
		hash = hash*101 + c;
	}
	return hash;
}

static inline const char *hex_unprintable(const char *sb)
{
	static const char hex[] = "0123456789abcdef";
	struct strbuf out = STRBUF_INIT;
	size_t len = strlen(sb);
	const char *c;

	for (c = sb; c < sb + len; c++) {
		if (isprint(*c)) {
			strbuf_addch(&out, *c);
		}
		else {
			strbuf_addch(&out, '\\');
			strbuf_addch(&out, hex[(unsigned char)*c >> 4]);
			strbuf_addch(&out, hex[*c & 0xf]);
		}
	}

	return strbuf_detach(&out, NULL);
}

/*#define max_comment 8192
static unsigned int strcmp_whitesp_ignore(const char *str1, const char *str2)
{
	char buf1[max_comment];
	char buf2[max_comment];
	char *p1 = buf1;
	char *p2 = buf2;
	const char *o1 = str1;
	const char *o2 = str2;

	if (strlen(str1) >= max_comment || strlen(str2) >= max_comment)
		die("strcmp_whitesp_ignore buf too small");

	while (*str1) {
		if (!isspace(*str1))
			*p1++ = tolower(*str1);
		str1++;
	}
	*p1 = '\0';

	while (*str2) {
		if (!isspace(*str2))
			*p2++ = tolower(*str2);
		str2++;
	}
	*p2 = '\0';

	int rc;
	rc = strcmp(buf1, buf2);
	if (rc != 0)
		fprintf(stderr, "cmp: %d\n'%s'\n'%s'\n'%s'\n'%s'\n", rc, hex_unprintable(buf1), hex_unprintable(buf2), hex_unprintable(o1), hex_unprintable(o2));

	return rc;
}*/

static unsigned int strcmp_whitesp_ignore(const char *str1, const char *str2)
{
	static struct strbuf buf1 = STRBUF_INIT;
	static struct strbuf buf2 = STRBUF_INIT;
	const char *o1 = str1;
	const char *o2 = str2;

	strbuf_grow(&buf1, strlen(str1));
	strbuf_grow(&buf2, strlen(str2));

	char *p1 = buf1.buf;
	char *p2 = buf2.buf;

	while (*str1) {
		if (!isspace(*str1))
			*p1++ = tolower(*str1);
		str1++;
	}
	*p1 = '\0';
	strbuf_setlen(&buf1, p1 - buf1.buf);

	while (*str2) {
		if (!isspace(*str2))
			*p2++ = tolower(*str2);
		str2++;
	}
	*p2 = '\0';
	strbuf_setlen(&buf2, p2 - buf2.buf);

	int rc;
	rc = strbuf_cmp(&buf1, &buf2);
	if (rc != 0)
		fprintf(stderr, "cmp: %d\n'%s'\n'%s'\n'%s'\n'%s'\n", rc, hex_unprintable(buf1.buf), hex_unprintable(buf2.buf), hex_unprintable(o1), hex_unprintable(o2));

	return rc;
}

static void revision_list_add(struct file_revision *rev, struct file_revision_list *list)
{
	if (list->size == list->nr) {
		if (list->size == 0)
			list->size = 64;
		else
			list->size *= 2;
		list->item = xrealloc(list->item,
				      list->size * sizeof(void *));
	}
	list->item[list->nr++] = rev;
}

void add_file_revision(struct branch_meta *meta,
		       const char *path,
		       const char *revision,
		       const char *author,
		       const char *msg,
		       time_t timestamp,
		       int isdead)
{
	/*
	 * make and init file_revision structure
	 * copy path and revision, timestamp, isdead
	 * get author + msg hash
	 * check patchset_hash hash and save patchset
	 *	or create patchset with author_msg
	 *
	 * add to list
	 */
	struct file_revision *rev;
	struct patchset *patchset;
	unsigned int hash;

	/*
	 * TODO:
	 * check import date, check branch last imported commit,
	 * compare and skip earlier revisions
	 */
	/*int rev1_br;
	int rev1_ver;
	int rev1_len;
	int rev2_br;
	int rev2_ver;
	int rev2_len;

	rev1_len = get_branch_rev(rev1, &rev1_br, &rev1_ver);
	rev2_len = get_branch_rev(rev2, &rev2_br, &rev2_ver);

	if (rev1_len == -1 || rev2_len == -1 || rev1_len != rev2_len ||
	    strncmp(rev1, rev2, rev1_len) != 0)
		return 0;

	if (rev1_br == rev2_br) {
		if (rev1_ver + 1 == rev2_ver)
			return 1;
		return 0;
	}

	if (rev2_ver == 0 && rev1_br + 1 == rev2_br)
		return 1;*/

	rev = xcalloc(1, sizeof(struct file_revision));
	rev->path = xstrdup(path);
	rev->revision = xstrdup(revision);
	rev->timestamp = timestamp;
	rev->isdead = isdead;

	hash = hash_author_msg(author, msg);
	patchset = lookup_hash(hash, meta->patchset_hash);
	if (patchset == NULL) {
		ps++;
		patchset = xcalloc(1, sizeof(struct patchset));
		patchset->author = xstrdup(author);
		patchset->msg = xstrdup(msg);
		insert_hash(hash, patchset, meta->patchset_hash);
	}
	else {
		if (strcmp(author, patchset->author) !=0 ||
		    strcmp_whitesp_ignore(msg, patchset->msg) != 0) {
			die("patchset author/message hash collision");
		}
	}
	rev->patchset = patchset;
	revision_list_add(rev, meta->rev_list);
}

time_t find_first_commit_time(struct branch_meta *meta)
{
	time_t min;
	int i;

	if (!meta->rev_list->nr)
		return 0;

	min = meta->rev_list->item[0]->timestamp;
	for (i = 1; i < meta->rev_list->nr; i++) {
		//fprintf(stderr, "min search %s %s %s\n", meta->rev_list->item[i]->path,
		//	meta->rev_list->item[i]->revision, show_date(meta->rev_list->item[i]->timestamp, 0, DATE_RFC2822));
		if (min > meta->rev_list->item[i]->timestamp)
			min = meta->rev_list->item[i]->timestamp;
	}

	return min;
}

int get_patchset_count(struct branch_meta *meta)
{
	int psnum = 0;
	struct patchset *ps = meta->patchset_list->head;

	while (ps) {
		psnum++;
		ps = ps->next;
	}

	return psnum;
}

static void patchset_list_add(struct patchset *patchset, struct patchset_list *list)
{
	if (list->head)
		list->tail->next = patchset;
	else
		list->head = patchset;
	list->tail = patchset;
}

static void patchset_add_file_revision(struct file_revision *rev, struct patchset *patchset)
{
	void **pos;
	unsigned int hash;

	patchset->timestamp_last = rev->timestamp;

	if (!patchset->timestamp) {
		// fresh patchset

		patchset->timestamp = rev->timestamp;
		patchset->revision_hash = xcalloc(1, sizeof(struct hash_table));
	}

	hash = hash_path(rev->path);
	pos = insert_hash(hash, rev, patchset->revision_hash);
	if (pos) {
		struct file_revision *old = *pos;
		if (strcmp(old->path, rev->path))
			die("patchset member path hash collision");
		//TODO: show merged revisions
		*pos = rev;
		rev->ismerged = 1;
	}
}

extern void print_ps(struct patchset *ps);
//dummy
static int validate_patchset_order(void *ptr, void *data)
{
	struct file_revision *rev = ptr;

	while (rev->prev && !rev->prev->ismeta) {
		if (rev->patchset->timestamp < rev->prev->patchset->timestamp) {
			die("patchset order is wrong");
		}
		else if (rev->patchset->timestamp == rev->prev->patchset->timestamp &&
			 rev->patchset != rev->prev->patchset) {
			struct patchset *patchset;
			int valid = 0;

			patchset = rev->prev->patchset;
			while (patchset && patchset->next) {
				if (patchset->next == rev->patchset) {
					valid = 1;
					break;
				}
				patchset = patchset->next;
			}
			if (!valid) {
				fprintf(stderr, "ps1\n");
				//print_ps(rev->prev->patchset);
				fprintf(stderr, "ps2\n");
				//print_ps(rev->patchset);
				error("same date patchsets order is wrong");
			}
		}
		rev = rev->prev;
	}
	return 0;
}

int validate_patchsets(struct branch_meta *meta)
{
	return for_each_hash(meta->revision_hash, validate_patchset_order, NULL);
}

void find_safe_cancellation_points(struct branch_meta *meta)
{
	time_t max_timestamp = 0;

	/*
	 * a commit is safe cancellation point if
	 * it has no intersection with other commits
	 */
	struct patchset *patchset = meta->patchset_list->head;

	while (patchset) {
		if (max_timestamp < patchset->timestamp_last)
			max_timestamp = patchset->timestamp_last;

		if (!patchset->next ||
		    max_timestamp < patchset->next->timestamp) {
			patchset->cancellation_point = max_timestamp;
			meta->last_revision_timestamp = max_timestamp;
		}

		patchset = patchset->next;

		/*if (!patchset->next) {
			patchset->cancellation_point
				= max_timestamp > patchset->timestamp_last
				? max_timestamp : patchset->timestamp_last;
			break;
		}

		if (max_timestamp < patchset->timestamp &&
		    patchset->timestamp_last < patchset->next->timestamp)*/
	}
}

void arrange_commit_time(struct branch_meta *meta)
{
	time_t max_timestamp = 0;

	struct patchset *patchset = meta->patchset_list->head;

	while (patchset) {
		if (max_timestamp < patchset->timestamp_last)
			max_timestamp = patchset->timestamp_last;
		else
			patchset->timestamp_last = max_timestamp;

		patchset = patchset->next;
	}
}

static struct patchset *split_patchset(struct patchset *patchset, struct hash_table *patchset_hash)
{
	unsigned int hash;
	struct patchset *new;
	void **pos;

	hash = hash_author_msg(patchset->author, patchset->msg);

	ps++;
	new = xcalloc(1, sizeof(struct patchset));
	new->author = xstrdup(patchset->author);
	new->msg = xstrdup(patchset->msg);

	pos = insert_hash(hash, new, patchset_hash);
	if (pos)
		*pos = new;
	return new;
}

int get_branch_rev(const char *rev, int *br, int *ver)
{
	const char *p = rev;
	const char *p1 = NULL;
	const char *p2 = NULL;
	while (*p) {
		if (*p == '.') {
			p1 = p2;
			p2 = p + 1;
		}
		++p;
	}

	if (!p2)
		return -1;

	if (!p1)
		p1 = rev;

	*br = atoi(p1);
	*ver = atoi(p2);
	return p1 - rev;
}

int is_prev_branch(const char *rev1, const char *rev2)
{
	size_t len1 = strlen(rev1);
	size_t len2 = strlen(rev2);

	if (len1 < len2 &&
	    rev2[len1] == '.' &&
	    !strncmp(rev1, rev2, len1))
		return 1;
	return 0;
}

/*
 * returns 1 is rev1 is previos revision for rev2
 * cases:
 *	1.2.3.9 -> 1.2.3.10
 *	1.2.3.9 -> 1.2.4.0
 *	1.2 -> 1.2.2.1
 */
int is_prev_rev(const char *rev1, const char *rev2)
{
	int rev1_br;
	int rev1_ver;
	int rev2_br;
	int rev2_ver;
	int rev1_len;
	int rev2_len;

	rev1_len = get_branch_rev(rev1, &rev1_br, &rev1_ver);
	rev2_len = get_branch_rev(rev2, &rev2_br, &rev2_ver);

	if (rev1_len == -1 || rev2_len == -1)
		return 0;

	if (rev1_len != rev2_len) {
		if (is_prev_branch(rev1, rev2))
			return 1;

		if (strncmp(rev1, rev2, rev1_len))
			return 0;
	}

	if (rev1_br == rev2_br) {
		if (rev1_ver + 1 == rev2_ver)
			return 1;
		return 0;
	}

	if (rev2_ver == 0 && rev1_br + 1 == rev2_br)
		return 1;
	return 0;
}

static int compare_rev_by_timestamp(const void *p1, const void *p2)
{
	const struct file_revision *rev1 = *(void**)p1;
	const struct file_revision *rev2 = *(void**)p2;

	if (rev1->timestamp < rev2->timestamp)
		return -1;
	else if (rev1->timestamp > rev2->timestamp)
		return 1;

	return 0;
}

void aggregate_patchsets(struct branch_meta *meta)
{
	// sort revisions list
	// for each revision:
	//	get revision_hash or check metadata
	//	fill in prev
	//	get patchset_hash hash
	//		check if empty -> add to patchset list
	//		or start new if prev is in newer patchset -> add to patchset list
	//		adjust patchset timestamps and add revision to hash
	//	adjust patchset
	//
	// validate:
	//	iterate revision_hash and iterate list, check revisions sequence
	//	and patchset sequence
	//
	// find safe cancelation points
	//	iterate patchsets, maintain maxtime, is ps timestamp > maxtime -
	//	safe cancelation point

	int i;

	qsort(meta->rev_list->item, meta->rev_list->nr, sizeof(void *), compare_rev_by_timestamp);

	fprintf(stderr, "SORT DONE\n");
	for (i = 0; i < meta->rev_list->nr; i++) {
		struct file_revision *rev;
		struct patchset *patchset;
		unsigned int hash;
		void **pos;
		int split = 0;

		rev = meta->rev_list->item[i];

		// get last patchset for this author + msg
		hash = hash_author_msg(rev->patchset->author, rev->patchset->msg);
		patchset = lookup_hash(hash, meta->patchset_hash);

		hash = hash_path(rev->path);
		pos = insert_hash(hash, rev, meta->revision_hash);
		if (pos) {
			struct file_revision *prev = *pos;

			if (strcmp(rev->path, prev->path))
				die("file path hash collision");

			rev->prev = prev;
			if (!is_prev_rev(prev->revision, rev->revision)) {
				error("revision sequence is wrong: file: %s %s %s -> %s", rev->path, prev->path, prev->revision, rev->revision);
			}

			if (patchset->timestamp &&
			    patchset->timestamp < prev->patchset->timestamp) {
				split = 1;
			}
			*pos = rev;
		}
		else {
			//TODO: check metadata if previous revision is there
			struct file_revision_meta *prev_meta;
			prev_meta = lookup_hash(hash, meta->last_commit_revision_hash);
			if (prev_meta) {
				rev->prev = (struct file_revision *)prev_meta;
				if (!is_prev_rev(prev_meta->revision, rev->revision)) {
					die("meta revision sequence is wrong. path: %s %s->%s",
						rev->path, prev_meta->revision, rev->revision);
				}
			}
			else {
				int br;
				int ver;
				if (get_branch_rev(rev->revision, &br, &ver) == -1)
					die("bad revision format %s", rev->revision);

				// TODO:
				// is it good, but flood with error in current way of
				// ignoring files added to another branch
				// if (ver != 1)
				//	error("new file version is not 1: %s %s", rev->path, rev->revision);
				// TODO: check that file is new (revision .*1.1)
			}
		}

		if (split ||
		    (patchset->timestamp && patchset->timestamp + meta->fuzz_time < rev->timestamp)) {
			patchset = split_patchset(patchset, meta->patchset_hash);
		}
		rev->patchset = patchset;
		if (!patchset->timestamp)
			patchset_list_add(patchset, meta->patchset_list);
		patchset_add_file_revision(rev, patchset);
	}

	fprintf(stderr, "GONNA VALIDATE\n");
	validate_patchsets(meta);
	find_safe_cancellation_points(meta);
	//arrange_commit_time(meta);
	fprintf(stderr, "SLEEP ps=%ld\n", ps);
	//sleep(100);
}

static time_t get_commit_author_time(const char *branch_ref)
{
/*	struct pretty_print_context pctx = {0};
	struct strbuf author_ident = STRBUF_INIT;
	struct strbuf committer_ident = STRBUF_INIT;
	unsigned char sha1[20];
	struct commit *commit;

	if (get_sha1_commit(branch_ref, sha1))
		die("cannot find last commit on branch ref %s", branch_ref);

	commit = lookup_commit(sha1);
	if (parse_commit(commit))
		die("cannot parse commit %s", sha1_to_hex(sha1));

	format_commit_message(cm, "%an <%ae>", &author_ident, &pctx);
	format_commit_message(cm, "%cn <%ce>", &committer_ident, &pctx);*/
	return 0;
}

struct branch_meta *new_branch_meta(const char *branch_name)
{
	struct strbuf meta_ref = STRBUF_INIT;
	struct strbuf branch_ref = STRBUF_INIT;
	struct branch_meta *new;
	new = xcalloc(1, sizeof(struct branch_meta));

	new->rev_list = xcalloc(1, sizeof(struct file_revision_list));
	new->patchset_hash = xcalloc(1, sizeof(struct hash_table));
	new->revision_hash = xcalloc(1, sizeof(struct hash_table));
	new->patchset_list = xcalloc(1, sizeof(struct patchset_list));

	new->fuzz_time = 2*60*60; // 2 hours

	new->last_commit_revision_hash = xcalloc(1, sizeof(struct hash_table));

	strbuf_addf(&meta_ref, "%s%s", get_meta_ref_prefix(), branch_name);

	if (ref_exists(meta_ref.buf)) {
		strbuf_addf(&branch_ref, "%s%s", get_ref_prefix(), branch_name);
		load_cvs_revision_meta(new, branch_ref.buf, meta_ref.buf);

		if (!new->last_revision_timestamp)
			new->last_revision_timestamp = get_commit_author_time(branch_ref.buf);
	}

	strbuf_release(&meta_ref);
	strbuf_release(&branch_ref);
	return new;
}

int free_hash_entry(void *ptr, void *data)
{
	struct file_revision_meta *rev_meta = ptr;
	free(rev_meta->path);
	free(rev_meta->revision);
	free(rev_meta);
	return 0;
}

void free_branch_meta(struct branch_meta *meta)
{
	int i;

	for (i = 0; i < meta->rev_list->nr; i++) {
		struct file_revision *rev;
		rev = meta->rev_list->item[i];

		free(rev->path);
		free(rev->revision);
		free(rev);
	}

	struct patchset *patchset = meta->patchset_list->head;
	if (patchset) {
		while (patchset) {
			struct patchset *delme = patchset;
			patchset = patchset->next;

			free(delme->author);
			free(delme->msg);
			free_hash(delme->revision_hash);
			free(delme->revision_hash);
			free(delme);
		}
	}
	else {
	}

	if (meta->rev_list->item)
		free(meta->rev_list->item);
	free(meta->rev_list);

	free_hash(meta->patchset_hash);
	free(meta->patchset_hash);

	free_hash(meta->revision_hash);
	free(meta->revision_hash);

	free(meta->patchset_list);

	for_each_hash(meta->last_commit_revision_hash, free_hash_entry, NULL);
	free_hash(meta->last_commit_revision_hash);
	free(meta->last_commit_revision_hash);

	free(meta);
}

void meta_map_init(struct meta_map *map)
{
	map->size = 0;
	map->nr = 0;
	map->array = NULL;
}

void meta_map_add(struct meta_map *map, const char *branch_name, struct branch_meta *meta)
{
	ALLOC_GROW(map->array, map->nr + 1, map->size);

	map->array[map->nr].branch_name = xstrdup(branch_name);
	map->array[map->nr].meta = meta;
	++map->nr;
}

struct branch_meta *meta_map_find(struct meta_map *map, const char *branch_name)
{
	struct meta_map_entry *item;

	for_each_branch_meta(item, map)
		if (!strcmp(item->branch_name, branch_name))
			return item->meta;

	return NULL;
}

void meta_map_release(struct meta_map *map)
{
	struct meta_map_entry *item;

	if (!map->array)
		return;

	for_each_branch_meta(item, map) {
		free(item->branch_name);
		free_branch_meta(item->meta);
	}

	free(map->array);
	meta_map_init(map);
}

/*
 * metadata work
 */

char *read_note_of(unsigned char sha1[20], const char *notes_ref, unsigned long *size)
{
	struct notes_tree *t;
	const unsigned char *note;
	enum object_type type;
	char *buf;

	t = xcalloc(1, sizeof(*t));
	init_notes(t, notes_ref, combine_notes_overwrite, 0);
	note = get_note(t, sha1);
	if (!note)
		die(_("No note found for sha1 %s."), sha1_to_hex(sha1));

	//fprintf(stderr, "note %s:\n", sha1_to_hex(note));
	buf = read_sha1_file(note, &type, size);
	if (!buf)
		die("Cannot read sha1 %s", sha1_to_hex(note));
	//const char *show_args[3] = {"show", sha1_to_hex(note), NULL};

	free_notes(t);
	free(t);

	return buf;
}

char *read_ref_note(const char *commit_ref, const char *notes_ref, unsigned long *size)
{
	unsigned char sha1[20];
	if (get_sha1(commit_ref, sha1))
		die(_("Failed to resolve '%s' as a valid ref."), commit_ref);

	return read_note_of(sha1, notes_ref, size);
}

void add_file_revision_meta_hash(struct hash_table *meta_hash,
		       const char *path,
		       const char *revision,
		       int isdead,
		       int isexec,
		       int mark)
{
	void **pos;
	unsigned int hash;
	struct file_revision_meta *rev_meta;

	rev_meta = xcalloc(1, sizeof(*rev_meta));
	rev_meta->path = xstrdup(path);
	rev_meta->revision = xstrdup(revision);
	rev_meta->ismeta = 1;
	rev_meta->isdead = isdead;
	rev_meta->isexec = isexec;
	rev_meta->mark = mark;

	hash = hash_path(path);
	pos = insert_hash(hash, rev_meta, meta_hash);
	if (pos) {
		die("add_file_revision_meta collision");
		*pos = rev_meta;
	}
}

void add_file_revision_meta(struct branch_meta *meta,
		       const char *path,
		       const char *revision,
		       int isdead,
		       int isexec,
		       int mark)
{
	add_file_revision_meta_hash(meta->last_commit_revision_hash, path, revision, isdead, isexec, mark);
}

char *parse_meta_line(char *buf, unsigned long len, char **first, char **second, char *p)
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
}

int load_cvs_revision_meta(struct branch_meta *meta,
			   const char *commit_ref,
			   const char *notes_ref)
{
	char *buf;
	char *p;
	char *first;
	char *second;
	unsigned long size;

	buf = read_ref_note(commit_ref, notes_ref, &size);
	if (!buf)
		return -1;

	p = buf;
	while ((p = parse_meta_line(buf, size, &first, &second, p))) {
		if (strcmp(first, "--") == 0)
			break;
		fprintf(stderr, "option: %s=>%s\n", first, second);
		if (strcmp(first, "UPDATE") == 0) {
			meta->last_revision_timestamp = atol(second);
			if (meta->last_revision_timestamp == 0)
				die("cvs metadata next UPDATE time is wrong");
		}
	}

	while ((p = parse_meta_line(buf,size, &first, &second, p))) {
		//fprintf(stderr, "revinfo: %s=>%s\n", first, second);
		add_file_revision_meta(meta, second, first, 0, 0, 0);
	}

	//write_or_die(1, buf, size);
	//fprintf(stderr, "end\n");
	free(buf);
	return 0;
}

void commit_meta(struct notes_tree *t, const char *notes_ref)
{
	struct strbuf commit_msg = STRBUF_INIT;
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

	/*
	 * message
	 */
	strbuf_addstr(&commit_msg, "boo");
	if (commit_tree(&commit_msg, tree_sha1, parents, result_sha1, NULL, NULL))
		die("Failed to commit notes tree to database");
	strbuf_release(&commit_msg);

	/*
	 * reflog
	 */
	update_ref("boom", notes_ref, result_sha1, NULL, 0, DIE_ON_ERR);
}

int save_note_for(const char *commit_ref, const char *notes_ref, char *buf, unsigned long len)
{
	struct notes_tree *t;
	unsigned char note_sha1[20], commit_sha1[20];

	if (get_sha1(commit_ref, commit_sha1))
		die(_("Failed to resolve '%s' as a valid ref."), commit_ref);

	t = xcalloc(1, sizeof(*t));
	init_notes(t, notes_ref, combine_notes_overwrite, 0);

	if (write_sha1_file(buf, len, blob_type, note_sha1))
		error(_("unable to write note object"));

	if (add_note(t, commit_sha1, note_sha1, combine_notes_overwrite) != 0)
		die("add_note failed");

	commit_meta(t, notes_ref);

	free_notes(t);
	free(t);

	return 0;
}

static int save_revision_meta(void *ptr, void *data)
{
	struct file_revision *rev = ptr;
	struct strbuf *sb = data;

	strbuf_addf(sb, "%s:%s\n", rev->revision, rev->path);
	return 0;
}

int save_cvs_revision_meta(struct branch_meta *meta,
			   const char *commit_ref,
			   const char *notes_ref)
{
	struct strbuf sb;
	strbuf_init(&sb, meta->revision_hash->nr * 64);
	if (meta->last_revision_timestamp)
		strbuf_addf(&sb, "UPDATE:%ld\n", meta->last_revision_timestamp);
	strbuf_addstr(&sb, "-\n");

	for_each_hash(meta->revision_hash, save_revision_meta, &sb);

	save_note_for(commit_ref, notes_ref, sb.buf, sb.len);
	strbuf_release(&sb);
	return 0;
}
