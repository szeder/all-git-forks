#include "remote-svn.h"
#include "exec_cmd.h"
#include "refs.h"
#include "progress.h"
#include "quote.h"
#include "run-command.h"
#include "tag.h"
#include "diff.h"
#include "revision.h"
#include "cache-tree.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

#define MAX_CMTS_PER_WORKER 1000

int svndbg = 1;
int svn_max_requests = 1;
static const char *url, *relpath, *authors_file, *empty_log_message;
static struct strbuf refdir = STRBUF_INIT;
static struct strbuf uuid = STRBUF_INIT;
static int verbose = 1;
static int use_progress;
static int listrev = INT_MAX;
static enum eol svn_eol = EOL_UNSET;
static struct index_state svn_index;
static struct svn_proto *proto;
static struct remote *remote;
static struct credential defcred;
static struct progress *progress;
static struct refspec *refmap;
static const char **refmap_str;
static int refmap_nr, refmap_alloc;
static struct string_list refs = STRING_LIST_INIT_DUP;
static struct string_list path_for_ref = STRING_LIST_INIT_NODUP;
static struct string_list excludes = STRING_LIST_INIT_DUP;
static struct string_list relexcludes = STRING_LIST_INIT_DUP;

struct svn_entry {
	struct svn_entry *next;
	char *ident, *msg;
	int mlen, rev;
};

/* svnref holds all the info we have about an svn branch starting at
 * start with a given path. This info is stored in a ref in refs/svn
 * (and an optional .tag). The start may not be valid until an
 * associated ref is found (svn != NULL) or a copy has been found for
 * the fetch. Refs are loaded on demand in workers and on the start of
 * fetch/push in the main process. */
struct svnref {
	struct svnref *next;

	const char *path;
	struct commit *svn;
	int rev, start;

	struct svn_entry *cmts, *cmts_last;
	struct svnref *copysrc, *first_copier, *next_copier;
	int logrev, copyrev;

	struct string_list gitrefs;

	struct refspec *push_spec;
	struct commit *push_cmt;
	struct object *push_obj;

	unsigned int exists_at_head : 1;
	unsigned int cmt_log_started : 1;
	unsigned int cmt_log_finished : 1;
	unsigned int need_copysrc_log : 1;
	unsigned int copy_modified : 1;
	unsigned int force_push : 1;
};

static int config(const char *key, const char *value, void *dummy) {
	if (!strcmp(key, "svn.eol")) {
		if (value && !strcasecmp(value, "lf"))
			svn_eol = EOL_LF;
		else if (value && !strcasecmp(value, "crlf"))
			svn_eol = EOL_CRLF;
		else if (value && !strcasecmp(value, "native"))
			svn_eol = EOL_NATIVE;
		else
			svn_eol = EOL_UNSET;
		return 0;

	} else if (!strcmp(key, "svn.emptymsg")) {
		return git_config_string(&empty_log_message, key, value);

	} else if (!strcmp(key, "svn.maxrequests")) {
		svn_max_requests = git_config_int(key, value);
		return 0;

	} else if (!strcmp(key, "svn.authors")) {
		return git_config_string(&authors_file, key, value);

	} else if (!prefixcmp(key, "remote.")) {
		const char *sub = key + strlen("remote.");
		if (!prefixcmp(sub, remote->name)) {
			sub += strlen(remote->name);

			if (!strcmp(sub, ".maxrev")) {
				listrev = git_config_int(key, value);
				return 0;

			} else if (!strcmp(sub, ".map")) {
				if (!value) return -1;
				ALLOC_GROW(refmap_str, refmap_nr+1, refmap_alloc);
				refmap_str[refmap_nr++] = xstrdup(value);
				return 0;

			} else if (!strcmp(sub, ".exclude")) {
				if (!value) return -1;
				string_list_insert(&relexcludes, value);
				return 0;
			}
		}
	}

	return git_default_config(key, value, dummy);
}


static void trypause(void) {
	static int env = -1;

	if (env == -1) {
		env = getenv("GIT_REMOTE_SVN_PAUSE") != NULL;
	}

	if (env) {
		struct stat st;
		int fd = open("remote-svn-pause", O_CREAT|O_RDWR, 0666);
		close(fd);
		while (!stat("remote-svn-pause", &st)) {
			sleep(1);
		}
	}
}

static void *map_lookup(struct string_list *list, const char *key) {
	struct string_list_item *item;
	item = string_list_lookup(list, key);
	return item ? item->util : NULL;
}

static const char *refname(struct svnref *r) {
	static struct strbuf ref[2] = {STRBUF_INIT, STRBUF_INIT};
	static int bufnr;

	struct strbuf *b = &ref[bufnr++ & 1];
	const char *path = r->path;

	strbuf_reset(b);
	strbuf_add(b, refdir.buf, refdir.len);

	while (*path) {
		int ch = *(path++);
		strbuf_addch(b, bad_ref_char(ch) ? '_' : ch);
	}

	strbuf_addf(b, ".%d", r->start);
	return b->buf;
}

static const char *refpath(const char *s) {
	static struct strbuf ref = STRBUF_INIT;
	strbuf_reset(&ref);
	s += strlen(relpath);
	if (*s == '/')
		s++;
	while (*s) {
		int ch = *(s++);
		strbuf_addch(&ref, bad_ref_char(ch) ? '_' : ch);
	}
	return ref.buf;
}

static struct svnref *get_older_ref(struct svnref *r, int rev) {
	struct svnref *s;
	for (s = r; s != NULL; s = s->next) {
		if (rev >= s->start) {
			return s;
		}
	}

	s = xcalloc(1, sizeof(*s));
	s->path = r->path;

	/* the ref couldn't be found above so it must be older then all
	 * the existing refs */
	while (r->next) {
		r = r->next;
	}

	r->next = s;
	return s;
}

/* By default assume this is a request for the newest ref that fits. If
 * this later turns out false, we will split the ref, adding an older
 * one.
 */
static struct svnref *get_ref(const char *path, int rev) {
	struct string_list_item *item = string_list_insert(&refs, path);

	if (item->util) {
		return get_older_ref(item->util, rev);
	} else {
		struct svnref *r = xcalloc(1, sizeof(*r));
		r->path = item->string;
		item->util = r;
		return r;
	}
}

static struct svnref *get_push_ref(struct refspec *spec) {
	static struct strbuf buf = STRBUF_INIT;
	const char *path = map_lookup(&path_for_ref, spec->dst);

	if (!path) {
		char *p = apply_refspecs(refmap, refmap_nr, spec->dst);
		strbuf_reset(&buf);
		strbuf_addstr(&buf, relpath);
		strbuf_addch(&buf, '/');
		strbuf_addstr(&buf, p);
		clean_svn_path(&buf);
		free(p);

		path = buf.buf;
	}

	return get_ref(path, INT_MAX);
}

static void set_ref_start(struct svnref *r, int start) {
	struct svnref *s;

	if (r->start == start)
		return;

	if (r->start) {
		s = xcalloc(1, sizeof(*s));
		s->path = r->path;
		s->start = r->start;
		s->svn = r->svn;
		s->rev = r->rev;
		s->next = r->next;
		r->next = s;
	}

	r->svn = NULL;
	r->rev = 0;
	r->start = start;
}

static int load_ref_cb(const char* refname, const unsigned char* sha1, int flags, void* cb_data) {
	struct commit *svn;
	struct svnref *r;
	const char *ext = strrchr(refname, '.');
	int rev;

	if (!ext || !strcmp(ext, ".tag"))
		return 0;

	svn = lookup_commit(sha1);
	rev = get_svn_revision(svn);
	r = get_ref(get_svn_path(svn), rev);
	set_ref_start(r, atoi(ext + 1));
	r->rev = rev;
	r->svn = svn;

	return 0;
}






struct author {
	char *name, *mail;
	struct credential cred;
};

struct author* authors;
size_t authorn, authora;

static char *duptrim(const char *begin, const char *end) {
	while (begin < end && isspace(begin[0]))
		begin++;
	while (end > begin && isspace(end[-1]))
		end--;
	return xmemdupz(begin, end - begin);
}

static void parse_authors(void) {
	char *p, *data;
	struct stat st;
	int fd;

	if (!authors_file)
		return;

	fd = open(authors_file, O_RDONLY);
	if (fd < 0 || fstat(fd, &st))
		die("authors file %s doesn't exist", authors_file);

	p = data = xmallocz(st.st_size);
	if (read_in_full(fd, data, st.st_size) != st.st_size)
		die("read failed on authors");

	while (p && *p) {
		struct ident_split ident;
		char *user_begin, *user_end;

		/* skip over terminating condition of previous line */
		if (p > data)
			p++;

		user_begin = p;
		p += strcspn(p, "#\n=");

		if (*p == '#') {
			/* full line comment */
			p = strchr(p, '\n');
			continue;

		} else if (*p == '\0' || *p == '\n') {
			/* empty line */
			continue;

		} else if (*p == '=') {
			/* have entry - user_end includes the = */
			user_end = ++p;
			p += strcspn(p, "#\n");
		}


		if (!split_ident_line(&ident, user_end, p - user_end)) {
			struct author a;
			credential_init(&a.cred);
			credential_from_url(&a.cred, url);

			a.cred.username = duptrim(user_begin, user_end-1);
			a.name = duptrim(ident.name_begin, ident.name_end);
			a.mail = duptrim(ident.mail_begin, ident.mail_end);

			ALLOC_GROW(authors, authorn+1, authora);
			authors[authorn++] = a;
		}

		/* comment after entry */
		if (*p == '#') {
			p = strchr(p, '\n');
		}
	}

	free(data);
	close(fd);
}

static const char *svn_to_ident(const char *username, const char *time) {
	int i;
	struct author *a;
	struct strbuf buf = STRBUF_INIT;

	for (i = 0; i < authorn; i++) {
		a = &authors[i];
		if (!strcasecmp(username, a->cred.username)) {
			goto end;
		}
	}

	if (authors_file) {
		die("could not find username '%s' in %s\n"
				"Add a line of the form:\n"
				"%s = Full Name <email@example.com>\n",
				username,
				authors_file,
				username);
	}

	ALLOC_GROW(authors, authorn+1, authora);
	a = &authors[authorn++];
	credential_init(&a->cred);
	credential_from_url(&a->cred, url);

	a->cred.username = xstrdup(username);
	a->name = a->cred.username;

	strbuf_addf(&buf, "%s@%s", username, uuid.buf);
	a->mail = strbuf_detach(&buf, NULL);

end:
	return fmt_ident(a->name, a->mail, time, 0);
}

static struct credential *push_credential(struct object* obj, int use_cmt_msg) {
	struct ident_split s;
	char *data = NULL, *author = NULL;
	int i;

	if (!authors_file)
		return &defcred;

	if (obj->type == OBJ_COMMIT) {
		struct commit* cmt = (struct commit*) obj;
		if (!use_cmt_msg) {
			return &defcred;
		}
		if (parse_commit(cmt)) {
			goto err;
		}
		author = strstr(cmt->buffer, "\ncommitter ");
		if (!author) {
			author = strstr(cmt->buffer, "\nauthor ");
		}
	} else if (obj->type == OBJ_TAG) {
		enum object_type type;
		unsigned long size;
		data = read_sha1_file(obj->sha1, &type, &size);
		if (!data || type != OBJ_TAG) {
			goto err;
		}
		author = strstr(data, "\ntagger ");
	}

	if (!author) goto err;

	/* skip over "\ncommitter " etc */
	author += strcspn(author, " ") + 1;

	if (split_ident_line(&s, author, strcspn(author, "\n")))
		goto err;

	for (i = 0; i < authorn; i++) {
		struct author* a = &authors[i];
		int sz = s.mail_end - s.mail_begin;
		if (strncasecmp(a->mail, s.mail_begin, sz) || a->mail[sz])
			continue;

		free(data);

		/* use defcred as it should already have a password */
		if (!strcmp(a->cred.username, defcred.username)) {
			return &defcred;
		}

		credential_fill(&a->cred);
		return &a->cred;
	}

err:
	die("can not find author in %s", sha1_to_hex(obj->sha1));
}







static void do_connect(void) {
	int i;
	struct strbuf buf = STRBUF_INIT;
	const char *p = defcred.protocol;
	strbuf_addstr(&buf, url);

	if (!strcmp(p, "http") || !strcmp(p, "https") || !strcmp(p, "svn+http") || !strcmp(p, "svn+https")) {
		proto = svn_http_connect(remote, &buf, &defcred, &uuid);
	} else if (!strcmp(p, "svn")) {
		proto = svn_connect(&buf, &defcred, &uuid);
	} else {
		die("don't know how to handle url %s", url);
	}

	if (prefixcmp(url, buf.buf))
		die("server returned different url (%s) then expected (%s)", buf.buf, url);

	relpath = url + buf.len;
	strbuf_addf(&refdir, "refs/svn/%s", uuid.buf);

	for (i = 0; i < relexcludes.nr; i++) {
		strbuf_reset(&buf);
		strbuf_addstr(&buf, relpath);
		strbuf_addch(&buf, '/');
		strbuf_addstr(&buf, relexcludes.items[i].string);
		clean_svn_path(&buf);
		string_list_insert(&excludes, buf.buf);
	}

	strbuf_release(&buf);
}

static void add_list_dir(const char *path) {
	char *ref = apply_refspecs(refmap, refmap_nr, refpath(path));
	struct string_list_item *item = string_list_insert(&path_for_ref, ref);
	struct svnref *r = get_ref(path, listrev);
	r->exists_at_head = 1;
	item->util = (void*) r->path;
	printf("? %s\n", ref);
}

static void list(void) {
	int i, j, latest;
	struct strbuf buf = STRBUF_INIT;

	for_each_ref_in(refdir.buf, &load_ref_cb, NULL);

	latest = proto->get_latest();
	listrev = min(latest, listrev);

	printf("@refs/heads/master HEAD\n");

	if (!listrev)
		return;

	for (i = 0; i < refmap_nr; i++) {
		strbuf_reset(&buf);
		strbuf_addstr(&buf, relpath);

		if (*refmap[i].src) {
			strbuf_addch(&buf, '/');
			strbuf_addstr(&buf, refmap[i].src);
			clean_svn_path(&buf);
		}

		if (refmap[i].pattern) {
			struct string_list dirs = STRING_LIST_INIT_DUP;
			char *after = strchr(refmap[i].src, '*') + 1;
			int len = strrchr(buf.buf, '*') - buf.buf - 1;

			strbuf_setlen(&buf, len);
			proto->list(buf.buf, listrev, &dirs);

			for (j = 0; j < dirs.nr; j++) {
				if (!*dirs.items[j].string)
					continue;

				strbuf_setlen(&buf, len);
				strbuf_addstr(&buf, dirs.items[j].string);
				strbuf_addstr(&buf, after);

				if (string_list_has_string(&excludes, buf.buf))
					continue;

				if (!*after || proto->isdir(buf.buf, listrev)) {
					add_list_dir(buf.buf);
				}
			}

			string_list_clear(&dirs, 0);

		} else if (proto->isdir(buf.buf, listrev)) {
			add_list_dir(buf.buf);
		}
	}

	strbuf_release(&buf);
}






/* logs may appear multiple times in the the log list if there revision
 * gets expanded
 */
static struct svnref **logs;
static int log_nr, log_alloc;
static int cmts_to_fetch;

static void request_log(struct svnref *r, int rev) {
	if (rev <= r->logrev)
		return;

	if (!r->logrev) {
		ALLOC_GROW(logs, log_nr+1, log_alloc);
		logs[log_nr++] = r;
	} else if (r->cmt_log_finished) {
		r->cmt_log_started = 0;
	}

	r->logrev = rev;
}

int next_log(struct svn_log *l) {
	int i;
	for (i = 0; i < log_nr; i++) {
		struct svnref *r = logs[i];

		if (r->logrev <= r->rev)
			continue;

		if (!r->cmt_log_started) {
			memset(l, 0, sizeof(*l));
			l->ref = r;
			l->path = r->path;
			/* include the last revision we have on disk, so
			 * that we can distinguish a replace in the
			 * following commit */
			if (r->cmts_last) {
				l->start = r->cmts_last->rev;
			} else if (r->rev) {
				l->start = r->rev;
			} else {
				l->start = 1;
			}
			l->start = r->rev ? r->rev : 1;
			l->end = r->logrev;
			l->get_copysrc = 0;
			r->cmt_log_started = 1;
			return 0;
		}

		if (r->need_copysrc_log) {
			memset(l, 0, sizeof(*l));
			l->ref = r;
			l->path = r->path;
			l->start = r->start;
			l->end = r->start;
			l->get_copysrc = 1;
			l->copy_modified = 0;
			r->need_copysrc_log = 0;
			return 0;
		}
	}

	return -1;
}

void cmt_read(struct svn_log *l, int rev, const char *author, const char *time, const char *msg) {
	const char *ident = svn_to_ident(author, time);
	int ilen = strlen(ident), mlen = strlen(msg);
	struct svn_entry *e;

	e = malloc(sizeof(*e) + ilen + 1 + mlen + 1);
	e->ident = (char*) (e + 1);
	e->msg = e->ident + ilen + 1;
	e->mlen = mlen;
	e->rev = rev;

	memcpy(e->ident, ident, ilen + 1);
	memcpy(e->msg, msg, mlen + 1);

	e->next = l->cmts;
	l->cmts = e;
	if (!l->cmts_last)
		l->cmts_last = e;

	display_progress(progress, ++cmts_to_fetch);
}

void log_read(struct svn_log* l) {
	struct svnref *r = l->ref;

	if (!l->get_copysrc) {
		if (!r->cmt_log_started || !l->cmts)
			die("bug: unexpected log");

		r->need_copysrc_log = 0;

		if (l->cmts->rev > l->start || !r->start) {
			struct svnref *c, *next;

			/* the log stopped early so we need to find the
			 * copy src */
			set_ref_start(r, l->cmts->rev);
			r->need_copysrc_log = 1;
			r->cmts = l->cmts;
			r->cmts_last = l->cmts_last;

			/* this ref may not cover all the copiers,
			 * rebuild the copy list moving some of them to
			 * an older ref */
			c = r->first_copier;
			r->first_copier = NULL;
			while (c != NULL) {
				c->copysrc = get_older_ref(r, c->copyrev);

				next = c->next_copier;
				c->next_copier = c->copysrc->first_copier;
				c->copysrc->first_copier = c;

				request_log(c->copysrc, c->copyrev);

				c = next;
			}

		} else {
			/* dump the extra commit that is a copy of the
			 * last commit of the previous log */
			struct svn_entry *next = l->cmts->next;

			if (next && r->cmts_last) {
				r->cmts_last->next = next;
				r->cmts_last = l->cmts_last;
			} else if (next) {
				r->cmts = next;
				r->cmts_last = l->cmts_last;
			}

			display_progress(progress, --cmts_to_fetch);
		}

		if (l->end < r->logrev) {
			/* in the interim another ref has required more commits */
			r->cmt_log_started = 0;
		} else {
			r->cmt_log_finished = 1;
		}

	} else {
		r->copyrev = l->copyrev;
		r->copy_modified = l->copy_modified;

		if (l->copyrev) {
			r->copysrc = get_ref(l->copysrc, l->copyrev);
			r->next_copier = r->copysrc->first_copier;
			r->copysrc->first_copier = r;
			request_log(r->copysrc, l->copyrev);
		}
	}
}

static void read_logs(void) {
	if (use_progress)
		progress = start_progress("Counting commits", 0);

	proto->read_logs();
	stop_progress(&progress);
}




static int logs_fetched, cmts_fetched;
static int updates_done, updates_nr;

struct finished_update {
	struct finished_update *next;
	int nr;
	struct strbuf buf;
};

static struct finished_update *finished_updates;
static FILE *update_helper;

static void send_to_helper(struct strbuf *b) {
	if (b->len) {
		if (svndbg >= 2) {
			struct strbuf buf = STRBUF_INIT;
			quote_path_fully = 1;
			quote_c_style_counted(b->buf, b->len, &buf, NULL, 1);
			fprintf(stderr, "to svn helper: %s\n", buf.buf);
			strbuf_release(&buf);
		}
		fwrite(b->buf, 1, b->len, update_helper);
	}
}

void update_read(struct svn_update *u) {
	if (u->rev)
		display_progress(progress, ++cmts_fetched);

	if (u->nr == updates_done) {
		struct finished_update *f = finished_updates;

		send_to_helper(&u->head);
		send_to_helper(&u->tail);
		updates_done++;

		while (f && f->nr == updates_done) {
			struct finished_update *next = f->next;
			send_to_helper(&f->buf);
			strbuf_release(&f->buf);
			free(f);
			updates_done++;
			f = next;
		}

		finished_updates = f;
	} else {
		struct finished_update *g, *f = xcalloc(1, sizeof(*f));
		f->nr = u->nr;
		strbuf_init(&f->buf, 0);
		strbuf_add(&u->head, u->tail.buf, u->tail.len);
		strbuf_swap(&f->buf, &u->head);

		g = finished_updates;
		if (g && f->nr > g->nr) {
			while (g->next && f->nr > g->next->nr) {
				g = g->next;
			}
			f->next = g->next;
			g->next = f;
		} else {
			f->next = g;
			finished_updates = f;
		}
	}
}

int next_update(struct svn_update *u) {
	if (updates_nr >= MAX_CMTS_PER_WORKER)
		return -1;

	u->nr = updates_nr++;
	u->rev = 0;

	while (logs_fetched < log_nr) {
		struct svnref *r = logs[logs_fetched];
		struct strbuf *h = &u->head;
		struct strbuf *t = &u->tail;

		if (r->cmts) {
			struct svn_entry *c = r->cmts;
			struct svnref *copy = c->rev == r->start ? r->copysrc : NULL;

			u->path = r->path;
			u->rev = c->rev;
			u->new_branch = !r->rev;
			u->copy = copy ? copy->path : NULL;
			u->copyrev = copy ? r->copyrev : 0;

			if (copy && !r->copy_modified) {
				strbuf_addf(h, "branch %s %d %s %d",
					refname(copy), r->copyrev,
					refname(r), c->rev);

				arg_quote(h, r->path);
				arg_quote(h, c->ident);
				strbuf_addf(h, " %d\n", c->mlen);
				strbuf_add(h, c->msg, c->mlen);
				strbuf_complete_line(h);

			} else {
				if (copy) {
					strbuf_addf(h, "checkout %s %d\n",
						refname(copy), r->copyrev);
				} else if (r->rev) {
					strbuf_addf(h, "checkout %s %d\n",
						refname(r), r->rev);
				} else {
					strbuf_addstr(h, "reset\n");
				}


				strbuf_addf(t, "commit %s %d %d",
					refname(r), r->rev, c->rev);

				arg_quote(t, r->path);
				arg_quote(t, c->ident);
				strbuf_addf(t, " %d\n", c->mlen);
				strbuf_add(t, c->msg, c->mlen);
				strbuf_complete_line(t);
			}

			if (!c->next)
				r->cmts_last = NULL;

			r->rev = c->rev;
			r->cmts = c->next;
			free(c);
			return 0;
		}

		if (!r->cmts && r->gitrefs.nr) {
			int i;
			strbuf_addf(h, "report %s", refname(r));
			for (i = 0; i < r->gitrefs.nr; i++) {
				strbuf_addf(h, " %s", r->gitrefs.items[i].string);
			}
			strbuf_complete_line(h);
			string_list_clear(&r->gitrefs, 0);
		}

		logs_fetched++;
	}

	if (u->head.len) {
		update_read(u);
	}

	return -1;
}

static int cmp_svnref_start(const void *u, const void *v) {
	struct svnref *const *a = u;
	struct svnref *const *b = v;
	return (*a)->start - (*b)->start;
}

static void fetch(void) {
	/* sort by start so that copysrcs are requested first */
	qsort(logs, log_nr, sizeof(logs[0]), &cmp_svnref_start);

	logs_fetched = 0;
	cmts_fetched = 0;

	/* Farm the pending requests out to a subprocess so that we can
	 * run git gc --auto after each chunk. Note that after calling
	 * gc --auto, we can't rely on any locally loaded objects being
	 * valid.
	 */
	while (logs_fetched < log_nr) {
		static const char *gc_auto[] = {"gc", "--auto", NULL};
		static const char *remote_svn_helper[] = {"remote-svn--helper", NULL};

		struct child_process ch;

		updates_done = 0;
		updates_nr = 0;

		memset(&ch, 0, sizeof(ch));
		ch.argv = remote_svn_helper;
		ch.in = -1;
		ch.out = xdup(fileno(stdout));
		ch.git_cmd = 1;

		if (use_progress)
			progress = start_progress("Fetching commits", cmts_to_fetch);

		if (start_command(&ch))
			die("failed to launch worker");

		update_helper = xfdopen(ch.in, "wb");
		proto->read_updates();

		if (finished_updates)
			die("bug: updates not flushed out");

		fclose(update_helper);

		stop_progress(&progress);

		if (finish_command(&ch))
			die_errno("worker failed");

		memset(&ch, 0, sizeof(ch));
		ch.argv = gc_auto;
		ch.no_stdin = 1;
		ch.no_stdout = 1;
		ch.git_cmd = 1;
		if (run_command(&ch))
			die_errno("git gc --auto failed");
	}
}








static int pushoff;

static void send_file(const char *path, const unsigned char *sha1, int create) {
	struct strbuf diff = STRBUF_INIT;
	struct strbuf buf = STRBUF_INIT;
	char *data;
	unsigned long sz;
	enum object_type type;
	struct cache_entry* ce;
	unsigned char nsha1[20];

	data = read_sha1_file(sha1, &type, &sz);
	if (type != OBJ_BLOB)
		die("unexpected object type for %s", sha1_to_hex(sha1));

	if (svn_eol != EOL_UNSET && convert_to_working_tree(path + pushoff + 1, data, sz, &buf)) {
		free(data);
		data = strbuf_detach(&buf, &sz);

		if (write_sha1_file(data, sz, "blob", nsha1)) {
			die_errno("blob write");
		}

		sha1 = nsha1;
	}

	ce = make_cache_entry(0644, sha1, path + pushoff + 1, 0, 0);
	add_index_entry(&svn_index, ce, ADD_CACHE_OK_TO_ADD);

	create_svndiff(&diff, data, sz);
	proto->send_file(path, &diff, create);

	free(data);
	strbuf_release(&diff);
}

static void change(struct diff_options* op,
		unsigned omode,
		unsigned nmode,
		const unsigned char* osha1,
		const unsigned char* nsha1,
		int osha1_valid,
		int nsha1_valid,
		const char* path,
		unsigned odsubmodule,
		unsigned ndsubmodule)
{
	struct cache_entry *ce;

	if (svndbg) fprintf(stderr, "change mode %x/%x sha1 %s/%s path %s\n",
			omode, nmode, sha1_to_hex(osha1), sha1_to_hex(nsha1), path);

	/* dont care about changed directories */
	if (!S_ISREG(nmode)) return;

	/* does the file exist in svn and not just git */
	ce = index_name_exists(&svn_index, path + pushoff + 1, strlen(path+pushoff+1), 0);
	if (!ce) return;

	send_file(path, nsha1, 0);

	/* TODO make this actually use diffcore */
	diff_change(op, omode, nmode, osha1, nsha1, osha1_valid, nsha1_valid, path, odsubmodule, ndsubmodule);
}

static void addremove(struct diff_options *op,
		int addrm,
		unsigned mode,
		const unsigned char *sha1,
		int sha1_valid,
		const char *path,
		unsigned dsubmodule)
{
	if (svndbg) fprintf(stderr, "addrm %c mode %x sha1 %s path %s\n",
			addrm, mode, sha1_to_hex(sha1), path);

	if (addrm == '-') {
		/* only push the delete if we have something to remove
		 * in svn */
		if (!remove_path_from_index(&svn_index, path + pushoff + 1)) {
			proto->delete(path);
		}

	} else if (S_ISDIR(mode)) {
		proto->mkdir(path);

	} else if (prefixcmp(strrchr(path, '/'), "/.git")) {
		/* files beginning with .git eg .gitempty,
		 * .gitattributes, etc are filtered from svn
		 */
		send_file(path, sha1, 1);
	}

	/* TODO use diffcore to track renames */
	diff_addremove(op, addrm, mode, sha1, sha1_valid, path, dsubmodule);
}

static const char *push_message(struct object *obj, int use_cmt_msg) {
	static struct strbuf buf = STRBUF_INIT;

	struct commit *cmt;
	const char *log;
	char *data;
	unsigned long size;
	enum object_type type;

	switch (obj->type) {
	case OBJ_COMMIT:
		cmt = (struct commit*) obj;
		if (!use_cmt_msg)
			return empty_log_message;

		if (parse_commit(cmt))
			die("invalid commit %s", sha1_to_hex(cmt->object.sha1));

		find_commit_subject(cmt->buffer, &log);
		return log;

	case OBJ_TAG:
		data = read_sha1_file(obj->sha1, &type, &size);
		find_commit_subject(data, &log);
		strbuf_reset(&buf);
		strbuf_add(&buf, log, parse_signature(log, data + size - log));
		free(data);
		return buf.buf;

	default:
		die("unexpected object type");
	}
}

static int push_commit(struct svnref *r, int type, struct object *obj, const char *specdst, int use_cmt_msg) {
	static struct strbuf buf = STRBUF_INIT;

	const char *ident, *log = push_message(obj, use_cmt_msg);
	int rev, copyrev = 0;
	const char *copypath = NULL;
	struct credential *cred = push_credential(obj, use_cmt_msg);
	unsigned char sha1[20];
	struct mergeinfo *mergeinfo, *svn_mergeinfo;
	struct commit *svnbase, *cmt;

	cmt = (struct commit*) deref_tag(obj, NULL, 0);
	if (parse_commit(cmt) || cmt->object.type != OBJ_COMMIT)
		die("invalid push object %s", sha1_to_hex(obj->sha1));

	proto->change_user(cred);

	if (cmt->parents) {
		struct commit_list *pp = cmt->parents;
		struct commit *sc = pp->item->util;

		svnbase = sc;

		if (type == SVN_ADD || type == SVN_REPLACE) {
			copypath = get_svn_path(sc);
			copyrev = get_svn_revision(sc);
		}

		/* mergeinfo contains the svn revs that follow the copy
		 * path (first parent), svn_mergeinfo is all paths from
		 * obj minus the direct path */

		mergeinfo = get_mergeinfo(sc);
		svn_mergeinfo = get_svn_mergeinfo(sc);
		pp = pp->next;

		while (pp) {
			struct mergeinfo *mi;
			sc = pp->item->util;

			mi = get_mergeinfo(sc);
			merge_svn_mergeinfo(svn_mergeinfo, mi, mergeinfo);
			free_svn_mergeinfo(mi);

			mi = get_svn_mergeinfo(sc);
			merge_svn_mergeinfo(svn_mergeinfo, mi, mergeinfo);
			free_svn_mergeinfo(mi);

			pp = pp->next;
		}

	} else {
		svnbase = NULL;
		mergeinfo = parse_svn_mergeinfo("");
		svn_mergeinfo = parse_svn_mergeinfo("");
	}


	/* need to checkout the git commit in order to get the
	 * .gitattributes files used for eol conversion
	 */
	svn_checkout_index(&the_index, cmt);
	svn_checkout_index(&svn_index, svnbase);

	proto->start_commit(type, log, r->path, max(r->rev, copyrev) + 1, copypath, copyrev, svn_mergeinfo);

	if (obj) {
		struct diff_options op;
		diff_setup(&op);
		op.output_format = DIFF_FORMAT_NO_OUTPUT;
		op.change = &change;
		op.add_remove = &addremove;
		DIFF_OPT_SET(&op, RECURSIVE);
		DIFF_OPT_SET(&op, IGNORE_SUBMODULES);
		DIFF_OPT_SET(&op, TREE_IN_RECURSIVE);

		strbuf_reset(&buf);
		strbuf_addstr(&buf, r->path);
		strbuf_addch(&buf, '/');

		pushoff = buf.len - 1;

		if (svnbase) {
			if (diff_tree_sha1(svn_commit(svnbase)->object.sha1, obj->sha1, buf.buf, &op))
				die("diff tree failed");
		} else {
			if (diff_root_tree_sha1(obj->sha1, buf.buf, &op))
				die("diff tree failed");
		}

		diffcore_std(&op);
		diff_flush(&op);
	}

	strbuf_reset(&buf);
	rev = proto->finish_commit(&buf);
	ident = svn_to_ident(cred->username, buf.buf);

	if (rev <= 0) {
		/* TODO get the full error message */
		printf("error %s commit failed\n", specdst);
		return -1;
	}

	/* If we find any intermediate commits, we complain. They will
	 * be picked up the next time the user does a pull.
	 */
	if (type == SVN_MODIFY && r->rev+1 < rev
		&& proto->has_change(r->path, r->rev+1, rev-1))
	{
		printf("error %s non-fast forward\n", specdst);
		return -1;
	}

	add_svn_mergeinfo(mergeinfo, r->path, r->rev+1, rev);

	/* update the svn ref */

	if (!svn_index.cache_tree)
		svn_index.cache_tree = cache_tree();
	if (cache_tree_update(svn_index.cache_tree, svn_index.cache, svn_index.cache_nr, 0))
		die("failed to update cache tree");

	if (write_svn_commit(r->svn, cmt, svn_index.cache_tree->sha1,
				ident, r->path, rev,
				mergeinfo, svn_mergeinfo,
				sha1)) {
		die("failed to write svn commit");
	}

	if (!r->start || type == SVN_ADD || type == SVN_REPLACE)
		set_ref_start(r, rev);

	strbuf_reset(&buf);
	strbuf_addstr(&buf, refname(r));
	update_ref("remote-svn", buf.buf, sha1,
			r->svn ? r->svn->object.sha1 : null_sha1,
			0, DIE_ON_ERR);

	r->exists_at_head = 1;
	r->rev = rev;
	r->svn = lookup_commit(sha1);

	/* update the svn tag ref */

	strbuf_addstr(&buf, ".tag");

	if (obj->type == OBJ_TAG) {
		if (read_ref(buf.buf, sha1)) {
			hashclr(sha1);
		}
		update_ref("remote-svn", buf.buf, obj->sha1, sha1, 0, DIE_ON_ERR);

	} else if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}

	free_svn_mergeinfo(mergeinfo);
	free_svn_mergeinfo(svn_mergeinfo);

	trypause();
	return 0;
}





#define SECOND_PARENT 1
#define FIRST_PARENT_NEW 2
#define FIRST_PARENT 3
#define IN_SVN 4
#define SVNCMT 5

static struct commit_list *push_list;
static int new_commits;

static void insert_commit(struct commit *item, void *util, int type)
{
	struct commit_list **pp = &push_list;
	int oldtype = item->object.flags;

	/* seen already by higher priority eg first parent or svn */
	if (oldtype >= type)
		return;

	if (oldtype) {
		/* we need to remove the commit from the list and
		 * reinsert so it gets placed in the correct position */

		if (oldtype < IN_SVN)
			new_commits--;

		for (pp = &push_list; *pp; pp = &(*pp)->next) {
			if ((*pp)->item == item) {
				*pp = (*pp)->next;
				break;
			}
		}
	}

	/* Sort by date then flags so svncmts are parsed before their
	 * pointed to commits. */
	for (pp = &push_list; *pp; pp = &(*pp)->next) {
		if ((*pp)->item->date < item->date) {
			break;
		}
		if ((*pp)->item->date == item->date && (*pp)->item->object.flags < type) {
			break;
		}
	}

	item->object.flags = type;
	item->util = util;
	commit_list_insert(item, pp);
}

static struct refspec **pushv;
static size_t pushn, pusha;

struct commit_dlist {
	struct commit_dlist *prev, *next;
	struct commit *item;
	void *util;
};

static void push(void) {
	struct commit_list *cmts = NULL;
	int i;

	/* push all svn heads */
	for (i = 0; i < refs.nr; i++) {
		struct svnref *r;
		for (r = refs.items[i].util; r != NULL; r = r->next) {
			insert_commit(r->svn, NULL, SVNCMT);
			insert_commit(svn_commit(r->svn), r->svn, IN_SVN);
		}
	}

	/* push all the new heads */
	for (i = 0; i < pushn; i++) {
		unsigned char sha1[20];
		struct svnref *r;
		int type;

		if (!*pushv[i]->src)
			continue;
		if (read_ref(pushv[i]->src, sha1) && get_sha1_hex(pushv[i]->src, sha1))
			die("invalid ref %s", pushv[i]->src);

		r = get_push_ref(pushv[i]);
		type = (r->exists_at_head) ? FIRST_PARENT : FIRST_PARENT_NEW;

		r->push_cmt = lookup_commit(sha1);
		r->push_obj = parse_object(sha1);

		insert_commit(r->push_cmt, pushv[i], type);
	}

	/* Date based search the tree from the svn and new heads marking
	 * commits with whether they need to be added to svn and if so
	 * onto which branch. The search stops when we've run out of
	 * non-svn commits by tracking the number of non-svn commits
	 * added.
	 */
	while (cmts && new_commits > 0) {
		struct commit *c = pop_commit(&cmts);
		int type = c->object.flags;

		if (type == SVNCMT) {
			struct commit *sc = svn_parent(c);
			insert_commit(sc, NULL, SVNCMT);
			insert_commit(svn_commit(sc), sc, IN_SVN);

		} else if (type != IN_SVN) {
			struct commit_list *pp = c->parents;
			for (pp = c->parents; pp != NULL; pp = pp->next) {
				struct commit *p = pp->item;

				if (parse_commit(p))
					die("invalid commit %s", p->object.sha1);

				if (pp == c->parents) {
					insert_commit(p, c->util, type);
				} else {
					insert_commit(p, NULL, SECOND_PARENT);
				}
			}
			new_commits--;
		}
	}

	/* Start pushing commits. We maintain a stack of commits in
	 * cmts to process. Commits can be included multiple times. If
	 * the head has non-svn parents those are pushed. If not, it is
	 * pushed to svn and popped. We repeat until we've processed all
	 * commits.
	 */

	for (i = 0; i < pushn; i++) {
		struct svnref *r = get_push_ref(pushv[i]);
		commit_list_insert(r->push_cmt, &cmts);
	}

	while (cmts) {
		struct commit *c = push_list->item;
		struct svnref *r;
		struct refspec *spec;
		struct commit_list *pp;
		int type;

		/* commits can appear multiple times in the stack and
		 * will have been pushed through to svn with the
		 * earliest occurrence
		 */
		if (c->object.flags == IN_SVN) {
			pop_commit(&push_list);
			continue;
		}

		/* if we have any parents which still need to be
		 * pushed, push them onto the stack and then we
		 * will get back to this commit once they've
		 * been processed */
		for (pp = c->parents; pp != NULL; pp = pp->next) {
			struct commit *p = pp->item;
			if (p->object.flags != IN_SVN) {
				commit_list_insert(p, &push_list);
			}
		}

		if (push_list->item != c)
			continue;

		spec = c->util;

		/* We may not find somewhere to put this commit if its
		 * only a second parent. If no twig folder is specified
		 * then we just don't push the commit.
		 */
		if (c->object.flags == SECOND_PARENT && !spec) {
			continue;
		}

		r = get_push_ref(spec);

		if (!r->exists_at_head) {
			type = SVN_ADD;
		} else if (!c->parents || c->parents->item != svn_commit(r->svn) || !prefixcmp(spec->dst, "refs/tags/")) {
			type = SVN_REPLACE;
		} else {
			type = SVN_MODIFY;
		}

		if (type == SVN_MODIFY && r->rev < listrev) {
			if (proto->has_change(r->path, r->rev+1, listrev)) {
				type = SVN_REPLACE;
			}
		}

		if (!spec->force && type == SVN_REPLACE) {
			printf("error %s non-fast forward\n", spec->dst);
			return;
		}

		/* we don't need to add the root on the initial commit */
		if (!*r->path && type == SVN_ADD) {
			type = SVN_MODIFY;
		}

		/* make sure we use the tag object for the final head */
		if (c == r->push_cmt) {
			if (push_commit(r, type, r->push_obj, spec->dst, 1))
				return;
		} else {
			if (push_commit(r, type, &c->object, spec->dst, 1))
				return;
		}

		pop_commit(&cmts);
	}

	/* Finally push through branch changes which weren't covered by
	 * pushing new commits: branch deletion and branch creation
	 * without a new commit (or the commit being pushed to a
	 * different branch).
	 */
	for (i = 0; i < pushn; i++) {
		struct svnref *r = get_push_ref(pushv[i]);

		if (!*pushv[i]->src) {
			proto->change_user(&defcred);
			proto->start_commit(SVN_DELETE, empty_log_message, r->path, r->rev+1, NULL, 0, NULL);

			if (proto->finish_commit(NULL) <= 0) {
				printf("error %s commit failed\n", pushv[i]->dst);
				return;
			}

			r->exists_at_head = 0;

		} else if (svn_commit(r->svn) != r->push_cmt) {

			if (r->exists_at_head) {
				if (push_commit(r, SVN_REPLACE, r->push_obj, pushv[i]->dst, 0))
					return;
			} else if (!r->exists_at_head) {
				if (push_commit(r, SVN_ADD, r->push_obj, pushv[i]->dst, 0))
					return;
			}
		}

		printf("ok %s\n", pushv[i]->dst);
	}
}








void arg_quote(struct strbuf *buf, const char *arg) {
	strbuf_addstr(buf, " \"");
	quote_c_style(arg, buf, NULL, 1);
	strbuf_addstr(buf, "\"");
}

static char* next_arg(char *arg, char **endp) {
	char *p;
	arg += strspn(arg, " ");
	p = arg + strcspn(arg, " \n");
	if (*p) *(p++) = '\0';
	*endp = p;
	return arg;
}

static int command(char *cmd, char *arg) {
	if (log_nr && !strcmp(cmd, "")) {
		read_logs();
		fetch();
		printf("\n");
		return 1;

	} else if (!strcmp(cmd, "fetch")) {
		char *ref, *path;
		struct svnref *r;

		next_arg(arg, &arg); /* sha1 */
		ref = next_arg(arg, &arg);
		path = map_lookup(&path_for_ref, strcmp(ref, "HEAD") ? ref : "refs/heads/master");

		if (!path)
			die("unexpected fetch ref %s", ref);

		r = get_ref(path, listrev);
		request_log(r, listrev);
		r->gitrefs.strdup_strings = 1;
		string_list_insert(&r->gitrefs, ref);

	} else if (pushn && !strcmp(cmd, "")) {
		int i;

		for (i = 0; i < refmap_nr; i++) {
			char *tmp = refmap[i].src;
			refmap[i].src = refmap[i].dst;
			refmap[i].dst = tmp;
		}

		push();
		printf("\n");
		return 1;

	} else if (!strcmp(cmd, "push")) {
		const char *ref = next_arg(arg, &arg);
		struct refspec *spec = parse_push_refspec(1, &ref);

		ALLOC_GROW(pushv, pushn+1, pusha);
		pushv[pushn++] = spec;

	} else if (!strcmp(cmd, "capabilities")) {
		printf("option\n");
		printf("fetch\n");
		printf("*fetch-unknown\n");
		printf("push\n");
		printf("\n");

	} else if (!strcmp(cmd, "option")) {
		char *opt = next_arg(arg, &arg);

		if (!strcmp(opt, "verbosity")) {
			verbose = strtol(arg, &arg, 10);
			printf("ok\n");
		} else if (!strcmp(opt, "progress")) {
			use_progress = !strcmp(next_arg(arg, &arg), "true");
			printf("ok\n");
		} else {
			printf("unsupported\n");
		}

		if (use_progress && verbose <= 1) {
			svndbg = 0;
		} else {
			svndbg = verbose;
		}

	} else if (!strcmp(cmd, "list")) {
		if (!strcmp(next_arg(arg, &arg), "for-push")) {
			credential_fill(&defcred);
		}
		do_connect();
		list();
		printf("\n");

	} else if (*cmd) {
		die("unexpected command %s", cmd);
	}

	return 0;
}

int main(int argc, const char **argv) {
	struct strbuf buf = STRBUF_INIT;

	trypause();

	git_extract_argv0_path(argv[0]);
	setup_git_directory();

	if (argc < 2)
		usage("git remote-svn remote [url]");

	remote = remote_get(argv[1]);
	if (!remote)
		die("invalid remote %s", argv[1]);

	if (argv[2]) {
		url = argv[2];
	} else if (remote && remote->url_nr) {
		url = remote->url[0];
	} else {
		die("no remote url");
	}

	git_config(&config, NULL);
	core_eol = svn_eol;

	/* svn commits are always in UTC, try and match them */
	setenv("TZ", "", 1);

	if (!prefixcmp(url, "svn::")) {
		url += strlen("svn::");
	}

	credential_init(&defcred);
	credential_from_url(&defcred, url);

	if (refmap_nr) {
		refmap = parse_fetch_refspec(refmap_nr, refmap_str);
	} else {
		refmap = xcalloc(1, sizeof(*refmap));
		refmap->src = (char*) "";
		refmap->dst = (char*) "refs/heads/master";
		refmap_nr = 1;
	}

	parse_authors();

	while (strbuf_getline(&buf, stdin, '\n') != EOF) {
		char *arg = buf.buf;
		char *cmd = next_arg(arg, &arg);
		if (command(cmd, arg))
			break;
		fflush(stdout);
	}

	return 0;
}

