#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vcs-cvs/aggregator.h"
#include "vcs-cvs/meta-store.h"
#include "cache.h"
#include "refs.h"

long ps = 0;

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

static void free_cvs_revision_list(struct cvs_revision_list *list)
{
	if (!list)
		return;

	if (list->item)
		free(list->item);
	free(list);
}

static struct cvs_revision *revision_list_add(struct cvs_revision *rev, struct cvs_revision_list *list)
{
	/*
	 * TODO: replace with ALLOC_GROW
	 */
	if (list->size == list->nr) {
		if (list->size == 0)
			list->size = 64;
		else
			list->size *= 2;
		list->item = xrealloc(list->item,
				      list->size * sizeof(void *));
	}
	list->item[list->nr++] = rev;
	return rev;
}

struct rev_sort_util {
	struct cvs_revision_list *lst1;
	struct cvs_revision_list *lst2;
	struct cvs_revision_list *cur_file;

	struct cvs_revision_list *sorted;
	char *cur_file_path;
	unsigned int sorted_files;
};

static struct rev_sort_util *init_rev_sort_util()
{
	struct rev_sort_util *su = xcalloc(1, sizeof(struct rev_sort_util));

	su->lst1 = xcalloc(1, sizeof(struct cvs_revision_list));
	su->lst2 = xcalloc(1, sizeof(struct cvs_revision_list));
	su->cur_file = xcalloc(1, sizeof(struct cvs_revision_list));

	return su;
}

static struct cvs_revision **rev_list_tail(struct cvs_revision_list *lst)
{
	return &lst->item[lst->nr];
}

static void merge_sort(struct cvs_revision_list *first,
		struct cvs_revision_list *second,
		struct cvs_revision_list *result)
{
	struct cvs_revision **first_it = &first->item[0];
	struct cvs_revision **second_it = &second->item[0];
	struct cvs_revision *last = NULL;

	while (first_it < rev_list_tail(first) &&
	       second_it < rev_list_tail(second)) {

		if ((*first_it)->timestamp > (*second_it)->timestamp) {
			last = revision_list_add(*first_it++, result);
		}
		else if ((*first_it)->timestamp < (*second_it)->timestamp) {
			last = revision_list_add(*second_it++, result);
		}
		else if (last && (*second_it)->cvs_commit == last->cvs_commit) {
			last = revision_list_add(*second_it++, result);
		}
		else {
			last = revision_list_add(*first_it++, result);
		}
	}

	while (first_it < rev_list_tail(first))
		revision_list_add(*first_it++, result);

	while (second_it < rev_list_tail(second))
		revision_list_add(*second_it++, result);

	first->nr = 0;
	second->nr = 0;
}

static void add_sort_cvs_revision(struct cvs_branch *meta, struct cvs_revision *rev)
{
	struct rev_sort_util *su;

	if (!meta->util)
		meta->util = init_rev_sort_util();

	su = meta->util;

	if (!su->cur_file_path) {
		su->cur_file_path = rev->path;
	}
	else if (strcmp(su->cur_file_path, rev->path)) {
		/*
		 * switched to next file
		 */
		if (!su->sorted) {
			su->sorted = su->cur_file;
			su->cur_file = su->lst1;
			su->lst1 = su->sorted;
		}
		else {
			struct cvs_revision_list *result = su->lst1;
			if (result == su->sorted)
				result = su->lst2;

			if (su->sorted_files > su->sorted->nr)
				die("su->sorted_files > su->sorted");
			su->sorted_files = su->sorted->nr;
			//fprintf(stderr, "%p sorted items %u adding %u file %s\n", meta, su->sorted->nr, su->cur_file->nr, su->cur_file->item[0]->path);
			merge_sort(su->sorted, su->cur_file, result);
			su->sorted = result;
			if (su->sorted_files > su->sorted->nr)
				die("su->sorted_files > su->sorted");
			su->sorted_files = su->sorted->nr;
		}
		su->cur_file_path = rev->path;
	}

	revision_list_add(rev, su->cur_file);
}

static struct cvs_revision_list *finish_rev_sort(struct cvs_branch *meta)
{
	struct rev_sort_util *su;
	struct cvs_revision_list *sorted;

	if (!meta->util)
		return xcalloc(1, sizeof(struct cvs_revision_list));

	su = meta->util;
	if (!su->sorted) {
		sorted = su->cur_file;
	}
	else {
		struct cvs_revision_list *result = su->lst1;
		if (result == su->sorted)
			result = su->lst2;

		merge_sort(su->sorted, su->cur_file, result);
		sorted = result;
	}

	if (sorted != su->lst1)
		free_cvs_revision_list(su->lst1);
	if (sorted != su->lst2)
		free_cvs_revision_list(su->lst2);
	if (sorted != su->cur_file)
		free_cvs_revision_list(su->cur_file);
	free(su);
	meta->util = NULL;
	return sorted;
}

int rev_cmp(const char *rev1, const char *rev2);
int add_cvs_revision(struct cvs_branch *meta,
		       const char *path,
		       const char *revision,
		       const char *author,
		       const char *msg,
		       time_t timestamp,
		       int isdead)
{
	/*
	 * make and init cvs_revision structure
	 * copy path and revision, timestamp, isdead
	 * get author + msg hash
	 * check cvs_commit_hash hash and save cvs_commit
	 *	or create cvs_commit with author_msg
	 *
	 * add to list
	 */
	struct cvs_revision *rev;
	struct cvs_commit *cvs_commit;
	struct cvs_revision *prev_meta;
	unsigned int hash;

	/*
	 * TODO:
	 * check import date, check branch last imported commit,
	 * compare and skip earlier revisions
	 */
	if (meta->last_revision_timestamp > timestamp)
		return 1;

	hash = hash_path(path);
	prev_meta = lookup_hash(hash, meta->last_commit_revision_hash);
	if (prev_meta) {
		if (strcmp(path, prev_meta->path))
			die("file path hash collision: \"%s\" \"%s\"", path, prev_meta->path);

		if (rev_cmp(prev_meta->revision, revision) >= 0)
			return 1;
	}

	rev = xcalloc(1, sizeof(struct cvs_revision));
	rev->path = xstrdup(path);
	rev->revision = xstrdup(revision);
	rev->timestamp = timestamp;
	rev->isdead = isdead;

	hash = hash_author_msg(author, msg);
	cvs_commit = lookup_hash(hash, meta->cvs_commit_hash);
	if (cvs_commit == NULL) {
		ps++;
		cvs_commit = xcalloc(1, sizeof(struct cvs_commit));
		cvs_commit->author = xstrdup(author);
		cvs_commit->msg = xstrdup(msg);
		insert_hash(hash, cvs_commit, meta->cvs_commit_hash);
	}
	else {
		if (strcmp(author, cvs_commit->author) !=0 ||
		    strcmp_whitesp_ignore(msg, cvs_commit->msg) != 0) {
			die("cvs_commit author/message hash collision");
		}
	}
	rev->cvs_commit = cvs_commit;

	add_sort_cvs_revision(meta, rev);
	return 0;
}

time_t find_first_commit_time(struct cvs_branch *meta)
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

int get_cvs_commit_count(struct cvs_branch *meta)
{
	int psnum = 0;
	struct cvs_commit *ps = meta->cvs_commit_list->head;

	while (ps) {
		psnum++;
		ps = ps->next;
	}

	return psnum;
}

static void cvs_commit_list_add(struct cvs_commit *cvs_commit, struct cvs_commit_list *list)
{
	if (list->head)
		list->tail->next = cvs_commit;
	else
		list->head = cvs_commit;
	list->tail = cvs_commit;
}

static void cvs_commit_add_revision(struct cvs_revision *rev, struct cvs_commit *cvs_commit)
{
	void **pos;
	unsigned int hash;

	cvs_commit->timestamp_last = rev->timestamp;

	if (!cvs_commit->timestamp) {
		// fresh cvs_commit

		cvs_commit->timestamp = rev->timestamp;
		cvs_commit->revision_hash = xcalloc(1, sizeof(struct hash_table));
	}

	hash = hash_path(rev->path);
	pos = insert_hash(hash, rev, cvs_commit->revision_hash);
	if (pos) {
		struct cvs_revision *old = *pos;
		if (strcmp(old->path, rev->path))
			die("cvs_commit member path hash collision");
		//TODO: show merged revisions
		*pos = rev;
		rev->ismerged = 1;
	}
}

static int print_cvs_revision(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;

	if (rev->prev) {
		struct cvs_revision *prev = rev->prev;
		while (prev && prev->ismerged && prev->prev)
			prev = prev->prev;
		if (prev->ismerged)
			fprintf(stderr, "\tunknown->%s-", rev->prev->revision);
		else
		fprintf(stderr, "\t%s->", rev->prev->revision);
	}
	else {
		fprintf(stderr, "\tunknown->");
	}
	fprintf(stderr, "%s\t%s", rev->revision, rev->path);

	if (rev->isdead)
		fprintf(stderr, " (dead)\n");
	else
		fprintf(stderr, "\n");
	return 0;
}

void print_cvs_commit(struct cvs_commit *commit)
{

	fprintf(stderr,
		"Author: %s\n"
		"AuthorDate: %s\n",
		commit->author,
		show_date(commit->timestamp, 0, DATE_NORMAL));
	fprintf(stderr,
		"CommitDate: %s\n",
		show_date(commit->timestamp_last, 0, DATE_NORMAL));
	fprintf(stderr,
		"UpdateDate: %s\n"
		"\n"
		"%s\n",
		show_date(commit->cancellation_point, 0, DATE_NORMAL),
		commit->msg);

	for_each_hash(commit->revision_hash, print_cvs_revision, NULL);
}

static int validate_cvs_commit_order(void *ptr, void *data)
{
	struct cvs_revision *rev = ptr;

	while (rev->prev && !rev->prev->ismeta) {
		if (rev->cvs_commit->timestamp < rev->prev->cvs_commit->timestamp) {
			die("cvs_commit order is wrong");
		}
		else if (rev->cvs_commit->timestamp == rev->prev->cvs_commit->timestamp &&
			 rev->cvs_commit != rev->prev->cvs_commit) {
			struct cvs_commit *cvs_commit;
			int valid = 0;

			cvs_commit = rev->prev->cvs_commit;
			while (cvs_commit && cvs_commit->next) {
				if (cvs_commit->next == rev->cvs_commit) {
					valid = 1;
					break;
				}
				cvs_commit = cvs_commit->next;
			}
			if (!valid) {
				fprintf(stderr, "commit1\n");
				print_cvs_commit(rev->cvs_commit);
				fprintf(stderr, "commit2\n");
				print_cvs_commit(rev->prev->cvs_commit);
				error("same date cvs_commits order is wrong file %s", rev->path);
			}
		}
		rev = rev->prev;
	}
	return 0;
}

int validate_cvs_commits(struct cvs_branch *meta)
{
	return for_each_hash(meta->revision_hash, validate_cvs_commit_order, NULL);
}

void find_safe_cancellation_points(struct cvs_branch *meta)
{
	time_t max_timestamp = 0;

	/*
	 * a commit is safe cancellation point if
	 * it has no intersection with other commits
	 */
	struct cvs_commit *cvs_commit = meta->cvs_commit_list->head;

	while (cvs_commit) {
		if (max_timestamp < cvs_commit->timestamp_last)
			max_timestamp = cvs_commit->timestamp_last;

		if (!cvs_commit->next ||
		    max_timestamp < cvs_commit->next->timestamp) {
			cvs_commit->cancellation_point = max_timestamp;
			meta->last_revision_timestamp = max_timestamp;
		}

		cvs_commit = cvs_commit->next;

		/*if (!cvs_commit->next) {
			cvs_commit->cancellation_point
				= max_timestamp > cvs_commit->timestamp_last
				? max_timestamp : cvs_commit->timestamp_last;
			break;
		}

		if (max_timestamp < cvs_commit->timestamp &&
		    cvs_commit->timestamp_last < cvs_commit->next->timestamp)*/
	}
}

void arrange_commit_time(struct cvs_branch *meta)
{
	time_t max_timestamp = 0;

	struct cvs_commit *cvs_commit = meta->cvs_commit_list->head;

	while (cvs_commit) {
		if (max_timestamp < cvs_commit->timestamp_last)
			max_timestamp = cvs_commit->timestamp_last;
		else
			cvs_commit->timestamp_last = max_timestamp;

		cvs_commit = cvs_commit->next;
	}
}

static struct cvs_commit *split_cvs_commit(struct cvs_commit *cvs_commit, struct hash_table *cvs_commit_hash)
{
	unsigned int hash;
	struct cvs_commit *new;
	void **pos;

	hash = hash_author_msg(cvs_commit->author, cvs_commit->msg);

	ps++;
	new = xcalloc(1, sizeof(struct cvs_commit));
	new->author = xstrdup(cvs_commit->author);
	new->msg = xstrdup(cvs_commit->msg);

	pos = insert_hash(hash, new, cvs_commit_hash);
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

/*
 * TODO: make it work with revisions from diffent branches
 */
int rev_cmp(const char *rev1, const char *rev2)
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
		die("bad revisions: \"%s\", \"%s\"", rev1, rev2);

	if (rev1_len != rev2_len) {
		if (is_prev_branch(rev1, rev2))
			return -1;

		if (is_prev_branch(rev2, rev1))
			return 1;

		die("cannot compare revisions from different branches: \"%s\", \"%s\"", rev1, rev2);
	}

	if (strncmp(rev1, rev2, rev1_len))
		die("cannot compare revisions from different branches: \"%s\", \"%s\"", rev1, rev2);

	if (rev1_br < rev2_br)
		return -1;
	else if (rev1_br > rev2_br)
		return 1;

	if (rev1_ver < rev2_ver)
		return -1;
	else if (rev1_ver > rev2_ver)
		return 1;

	return 0;
}

int compare_fix_rev(const void *p1, const void *p2)
{
	struct cvs_revision *rev1 = *(void**)p1;
	struct cvs_revision *rev2 = *(void**)p2;

	int is_same_file = !strcmp(rev1->path, rev2->path);

	if (is_same_file) {
		switch (rev_cmp(rev1->revision, rev2->revision)) {
		case -1:
			if (rev1->timestamp > rev2->timestamp) {
				error("file: %s revision: %s time: %ld is later then revision: %s time: %ld (fixing)",
					rev1->path, rev1->revision, rev1->timestamp, rev2->revision, rev2->timestamp);
				rev2->timestamp = rev1->timestamp;
			}
			return -1;
		case 1:
			if (rev2->timestamp > rev1->timestamp) {
				error("file: %s revision: %s time: %ld is later then revision: %s time: %ld (fixing)",
					rev2->path, rev2->revision, rev2->timestamp, rev1->revision, rev1->timestamp);
				rev1->timestamp = rev2->timestamp;
			}
			return 1;
		case 0:
			die("dup file: %s revision: %s", rev1->path, rev1->revision);
		}
	}

	if (rev1->timestamp < rev2->timestamp)
		return -1;
	else if (rev1->timestamp > rev2->timestamp)
		return 1;

	return 0;
}

void reverse_rev_list(struct cvs_revision_list *rev_list)
{
	struct cvs_revision *tmp;
	int i;

	for (i = 0; i < rev_list->nr / 2; i++) {
		tmp = rev_list->item[i];
		rev_list->item[i] = rev_list->item[rev_list->nr - 1 - i];
		rev_list->item[rev_list->nr - 1 - i] = tmp;
	}

	/*for (i = 0; i < rev_list->nr; i++) {
		tmp = rev_list->item[i];
		fprintf(stderr, "%p %s %s\n", tmp->cvs_commit, tmp->path, tmp->revision);
	}*/
}

void finalize_revision_list(struct cvs_branch *meta)
{
	meta->rev_list = finish_rev_sort(meta);
}

void aggregate_cvs_commits(struct cvs_branch *meta)
{
	// sort revisions list
	// for each revision:
	//	get revision_hash or check metadata
	//	fill in prev
	//	get cvs_commit_hash hash
	//		check if empty -> add to cvs_commit list
	//		or start new if prev is in newer cvs_commit -> add to cvs_commit list
	//		adjust cvs_commit timestamps and add revision to hash
	//	adjust cvs_commit
	//
	// validate:
	//	iterate revision_hash and iterate list, check revisions sequence
	//	and cvs_commit sequence
	//
	// find safe cancelation points
	//	iterate cvs_commits, maintain maxtime, is ps timestamp > maxtime -
	//	safe cancelation point

	unsigned int ps_seq = 0;
	int i;

	/*
	 * Revisions goes latest first in rlog. Sorting by date is broken when
	 * bunch of commits is pushed at the same time (timestamp is the same).
	 * Keeping commits in histitorical order helps to avoid extra cvs_commit
	 * splits.
	 * TODO: topological sort same second
	 */
	//qsort(meta->rev_list->item, meta->rev_list->nr, sizeof(void *), compare_fix_rev);
	reverse_rev_list(meta->rev_list);

	fprintf(stderr, "SORT DONE\n");
	for (i = 0; i < meta->rev_list->nr; i++) {
		struct cvs_revision *rev;
		struct cvs_commit *cvs_commit;
		unsigned int hash;
		void **pos;
		int split = 0;

		rev = meta->rev_list->item[i];

		// get last cvs_commit for this author + msg
		hash = hash_author_msg(rev->cvs_commit->author, rev->cvs_commit->msg);
		cvs_commit = lookup_hash(hash, meta->cvs_commit_hash);

		hash = hash_path(rev->path);
		pos = insert_hash(hash, rev, meta->revision_hash);
		if (pos) {
			struct cvs_revision *prev = *pos;

			if (strcmp(rev->path, prev->path))
				die("file path hash collision");

			rev->prev = prev;
			if (!is_prev_rev(prev->revision, rev->revision)) {
				//error("revision sequence is wrong: file: %s %s %s -> %s", rev->path, prev->path, prev->revision, rev->revision);
				die("revision sequence is wrong: file: %s %s %s -> %s", rev->path, prev->path, prev->revision, rev->revision);
			}

			if (cvs_commit->timestamp &&
			    //cvs_commit->timestamp <= prev->cvs_commit->timestamp) {
			    (cvs_commit->timestamp < prev->cvs_commit->timestamp ||
				    (cvs_commit->timestamp == prev->cvs_commit->timestamp &&
				     cvs_commit->seq < prev->cvs_commit->seq))) {
				split = 1;
			}
			*pos = rev;
		}
		else {
			//TODO: check metadata if previous revision is there
			struct cvs_revision *prev_meta;
			prev_meta = lookup_hash(hash, meta->last_commit_revision_hash);
			if (prev_meta) {
				rev->prev = (struct cvs_revision *)prev_meta;
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
		    (cvs_commit->timestamp && cvs_commit->timestamp + meta->fuzz_time < rev->timestamp)) {
			cvs_commit = split_cvs_commit(cvs_commit, meta->cvs_commit_hash);
		}
		rev->cvs_commit = cvs_commit;
		if (!cvs_commit->timestamp) {
			cvs_commit_list_add(cvs_commit, meta->cvs_commit_list);
			cvs_commit->seq = ++ps_seq;
		}
		cvs_commit_add_revision(rev, cvs_commit);
	}

	fprintf(stderr, "GONNA VALIDATE\n");
	/*struct cvs_commit *cvs_commit = meta->cvs_commit_list->head;
	i = 0;
	while (cvs_commit) {
		fprintf(stderr, "-> cvs_commit %d\n", ++i);
		print_cvs_commit(cvs_commit);
		cvs_commit = cvs_commit->next;
	}*/

	validate_cvs_commits(meta);
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

struct cvs_branch *new_cvs_branch(const char *branch_name)
{
	struct strbuf meta_ref = STRBUF_INIT;
	struct strbuf branch_ref = STRBUF_INIT;
	unsigned char sha1[20];
	struct cvs_branch *new;
	new = xcalloc(1, sizeof(struct cvs_branch));

	//new->rev_list = xcalloc(1, sizeof(struct cvs_revision_list));
	new->cvs_commit_hash = xcalloc(1, sizeof(struct hash_table));
	new->revision_hash = xcalloc(1, sizeof(struct hash_table));
	new->cvs_commit_list = xcalloc(1, sizeof(struct cvs_commit_list));

	new->fuzz_time = 2*60*60; // 2 hours

	strbuf_addf(&meta_ref, "%s%s", get_meta_ref_prefix(), branch_name);

	if (ref_exists(meta_ref.buf)) {
		strbuf_addf(&branch_ref, "%s%s", get_ref_prefix(), branch_name);
		if (get_sha1(branch_ref.buf, sha1))
			die(_("Failed to resolve '%s' as a valid ref."), branch_ref.buf);

		load_revision_meta(sha1, meta_ref.buf, &new->last_revision_timestamp, &new->last_commit_revision_hash);

		if (!new->last_revision_timestamp)
			new->last_revision_timestamp = get_commit_author_time(branch_ref.buf);
	}

	if (!new->last_commit_revision_hash)
		new->last_commit_revision_hash = xcalloc(1, sizeof(struct hash_table));

	strbuf_release(&meta_ref);
	strbuf_release(&branch_ref);
	return new;
}

int free_hash_entry(void *ptr, void *data)
{
	struct cvs_revision *rev_meta = ptr;
	free(rev_meta->path);
	free(rev_meta->revision);
	free(rev_meta);
	return 0;
}

void free_cvs_branch(struct cvs_branch *meta)
{
	int i;

	for (i = 0; i < meta->rev_list->nr; i++) {
		struct cvs_revision *rev;
		rev = meta->rev_list->item[i];

		free(rev->path);
		free(rev->revision);
		free(rev);
	}

	struct cvs_commit *cvs_commit = meta->cvs_commit_list->head;
	if (cvs_commit) {
		while (cvs_commit) {
			struct cvs_commit *delme = cvs_commit;
			cvs_commit = cvs_commit->next;

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

	free_hash(meta->cvs_commit_hash);
	free(meta->cvs_commit_hash);

	free_hash(meta->revision_hash);
	free(meta->revision_hash);

	free(meta->cvs_commit_list);

	for_each_hash(meta->last_commit_revision_hash, free_hash_entry, NULL);
	free_hash(meta->last_commit_revision_hash);
	free(meta->last_commit_revision_hash);

	free(meta);
}

