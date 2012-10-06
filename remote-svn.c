#include "remote-svn.h"
#include "exec_cmd.h"
#include "refs.h"
#include "progress.h"
#include "quote.h"
#include "run-command.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

#define MAX_CMTS_PER_WORKER 1000

int svndbg = 1;
static const char *url, *relpath, *authors_file;
static struct strbuf refdir = STRBUF_INIT;
static struct strbuf uuid = STRBUF_INIT;
static int verbose = 1;
static int use_progress;
static int listrev = INT_MAX;
static struct svn_proto *proto;
static struct remote *remote;
static struct credential defcred;
static struct progress *progress;
static struct refspec *refmap;
static const char **refmap_str;
static int refmap_nr, refmap_alloc;
static struct string_list refs = STRING_LIST_INIT_DUP;
static struct string_list path_for_ref = STRING_LIST_INIT_NODUP;

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

	unsigned int exists_at_head : 1;
	unsigned int cmt_log_started : 1;
	unsigned int cmt_log_finished : 1;
	unsigned int need_copysrc_log : 1;
	unsigned int copy_modified : 1;
};

static int config(const char *key, const char *value, void *dummy) {
	if (!strcmp(key, "svn.authors")) {
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
	rev = parse_svn_revision(svn);
	r = get_ref(parse_svn_path(svn), rev);
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







static void do_connect(void) {
	struct strbuf urlb = STRBUF_INIT;
	strbuf_addstr(&urlb, url);

	die("don't know how to handle url %s", url);

	if (prefixcmp(url, urlb.buf))
		die("server returned different url (%s) then expected (%s)", urlb.buf, url);

	relpath = url + urlb.len;
	strbuf_addf(&refdir, "refs/svn/%s", uuid.buf);

	strbuf_release(&urlb);
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

	for_each_ref_in(refdir.buf, &load_ref_cb, NULL);

	latest = proto->get_latest();
	listrev = min(latest, listrev);

	printf("@refs/heads/master HEAD\n");

	if (!listrev)
		return;

	for (i = 0; i < refmap_nr; i++) {
		struct strbuf buf = STRBUF_INIT;

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

				if (!*after || proto->isdir(buf.buf, listrev)) {
					add_list_dir(buf.buf);
				}
			}

			string_list_clear(&dirs, 0);

		} else if (proto->isdir(buf.buf, listrev)) {
			add_list_dir(buf.buf);
		}

		strbuf_release(&buf);
	}
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
	if (!strcmp(cmd, "") && log_nr) {
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

	} else if (!strcmp(cmd, "capabilities")) {
		printf("option\n");
		printf("fetch\n");
		printf("*fetch-unknown\n");
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

