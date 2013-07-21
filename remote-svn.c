/* vim: set noet ts=8 sw=8 sts=8: */
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

int svn_max_requests = 3;
int svndbg = 1;
static struct strbuf gitroot = STRBUF_INIT;
static const char *url, *relpath, *authors_file, *empty_log_message = "";
static struct strbuf refdir = STRBUF_INIT;
static struct strbuf uuid = STRBUF_INIT;
static int verbose = 1;
static int use_progress;
static int listrev = INT_MAX;
static int gcperiod = 1000;
static enum eol svn_eol = EOL_UNSET;
static struct index_state svn_index;
static struct svn_proto *proto;
static struct remote *remote;
static struct credential defcred;
static struct progress *progress;
static struct refspec *refmap;
static char **refmap_str;
static int refmap_nr, refmap_alloc;
static struct string_list refs = STRING_LIST_INIT_DUP;
static struct string_list path_for_ref = STRING_LIST_INIT_NODUP;
static struct string_list excludes = STRING_LIST_INIT_DUP;
static struct string_list relexcludes = STRING_LIST_INIT_DUP;
static struct strbuf twig_path = STRBUF_INIT;

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

	} else if (!strcmp(key, "svn.gcperiod")) {
		gcperiod = git_config_int(key, value);
		return 0;

	} else if (!strcmp(key, "svn.authors")) {
		return git_config_string(&authors_file, key, value);

	} else if (!strcmp(key, "svn.maxrequests")) {
		svn_max_requests = git_config_int(key, value);
		return 0;

	} else if (!strcmp(key, "svn.twigpath")) {
		strbuf_reset(&twig_path);
		if (value) {
			strbuf_addstr(&twig_path, value);
			string_list_insert(&relexcludes, value);
		}
		return 0;

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

			} else if (!strcmp(sub, ".gitpath")) {
				strbuf_reset(&gitroot);
				if (value) {
					strbuf_addstr(&gitroot, value);
					clean_svn_path(&gitroot);
				}
				return 0;
			}
		}
	}

	return git_default_config(key, value, dummy);
}

__attribute__((format (printf,1,2)))
static void remotef(const char *fmt, ...);

static void remotef(const char *fmt, ...) {
	va_list ap, aq;
	va_start(ap, fmt);
	if (svndbg >= 2) {
		static struct strbuf buf = STRBUF_INIT;
		va_copy(aq, ap);
		strbuf_reset(&buf);
		strbuf_addf(&buf, "R+ ");
		strbuf_vaddf(&buf, fmt, aq);
		strbuf_complete_line(&buf);
		fwrite(buf.buf, 1, buf.len, stderr);
	}
	vprintf(fmt, ap);
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
	static struct strbuf bufs[2] = {STRBUF_INIT, STRBUF_INIT};
	static int bufnr;

	struct strbuf *b = &bufs[bufnr++ & 1];
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

static const char *refszname(struct svnref *r) {
	static struct strbuf bufs[2] = {STRBUF_INIT, STRBUF_INIT};
	static int bufnr;

	struct strbuf *b = &bufs[bufnr++ & 1];
	const char *ref = refname(r);

	strbuf_reset(b);
	strbuf_addf(b, "%d:%s", (int) strlen(ref), ref);
	return b->buf;
}

static char *refpath(const char *path) {
	char *gitref, *s;
	path += strlen(relpath);
	if (*path == '/')
		path++;
	gitref = apply_refspecs(refmap, refmap_nr, path);
	for (s = gitref; *s; s++) {
		if (bad_ref_char(*s))
			*s = '_';
	}
	return gitref;
}

/* By default assume this is a request for the newest ref that fits. If
 * this later turns out false, we will split the ref, adding an older
 * one using set_ref_start.
 */
static struct svnref *get_ref(const char *path, int rev) {
	struct string_list_item *item = string_list_insert(&refs, path);
	struct svnref *r;

	/* search the list of revs for this path */
	for (r = item->util; r != NULL; r = r->next) {
		if (rev >= r->start) {
			return r;
		}
	}

	/* we may have to create a new oldest ref */
	r = xcalloc(1, sizeof(*r));
	r->path = item->string;
	r->gitrefs.strdup_strings = 1;

	if (item->util) {
		struct svnref *s = item->util;
		while (s->next) {
			s = s->next;
		}
		s->next = r;
	} else {
		item->util = r;
	}

	return r;
}

static struct svnref *get_push_ref(struct refspec *spec) {
	static struct strbuf buf = STRBUF_INIT;
	const char *path = map_lookup(&path_for_ref, spec->dst);

	if (!path) {
		char *p = apply_refspecs(refmap, refmap_nr, spec->dst);
		if (!p) {
			return NULL;
		}
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
		if (start <= r->start) {
			die("internal: split ref to older start");
		}

		s = xcalloc(1, sizeof(*s));
		s->gitrefs.strdup_strings = 1;
		s->path = r->path;
		s->start = r->start;
		s->svn = r->svn;
		s->rev = r->rev;
		s->logrev = r->rev;
		s->is_tag = r->is_tag;

		s->next = r->next;
		r->next = s;
	}

	r->svn = NULL;
	r->rev = 0;
	r->start = start;
}

static int load_ref_cb(const char* refname, const unsigned char* cmtsha1, int flags, void* cb_data) {
	struct strbuf buf = STRBUF_INIT;
	struct commit *svn;
	struct svnref *r;
	const char *ext = strrchr(refname, '.');
	unsigned char sha1[20];
	enum object_type type;
	size_t srcn;
	char *p;
	int rev;

	if (!ext || ext[1] < '0' || ext[1] > '9')
		return 0;

	svn = lookup_commit(cmtsha1);
	rev = get_svn_revision(svn);
	r = get_ref(get_svn_path(svn), rev);
	set_ref_start(r, atoi(ext + 1));
	r->rev = rev;
	r->svn = svn;

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s%s.log", refdir.buf, refname);

	if (!read_ref(buf.buf, sha1) && (p = read_sha1_file(sha1, &type, &srcn)) != NULL) {
		r->logrev = atoi(p);
		free(p);
	}

	if (r->logrev < rev) {
		r->logrev = rev;
	}

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s%s.tag", refdir.buf, refname);

	if (!read_ref(buf.buf, sha1)) {
		r->is_tag = 1;
	}

	strbuf_release(&buf);

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
		user_end = p;

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







static void do_connect(int ispush) {
	int i;
	struct strbuf buf = STRBUF_INIT;
	const char *p = defcred.protocol;
	strbuf_addstr(&buf, url);

	if (!strcmp(p, "http") || !strcmp(p, "https")) {
		proto = svn_http_connect(remote, &buf, &defcred, &uuid, ispush);
	} else if (!strcmp(p, "svn")) {
		proto = svn_proto_connect(&buf, &defcred, &uuid);
	} else {
		die("don't know how to handle url %s", url);
	}

	if (prefixcmp(url, buf.buf))
		die("server returned different url (%s) then expected (%s)", buf.buf, url);

	relpath = url + buf.len;
	strbuf_reset(&refdir);
	strbuf_addf(&refdir, "refs/svn/%s", uuid.buf);

	string_list_clear(&excludes, 0);

	for (i = 0; i < relexcludes.nr; i++) {
		strbuf_reset(&buf);
		strbuf_addstr(&buf, relpath);
		strbuf_addch(&buf, '/');
		strbuf_addstr(&buf, relexcludes.items[i].string);
		clean_svn_path(&buf);
		string_list_insert(&excludes, buf.buf);
	}

	if (twig_path.len) {
		strbuf_reset(&buf);
		strbuf_addstr(&buf, relpath);
		strbuf_addch(&buf, '/');
		strbuf_addstr(&buf, twig_path.buf);
		clean_svn_path(&buf);
		strbuf_swap(&buf, &twig_path);
	}

	strbuf_release(&buf);
}

static void add_list_dir(const char *path) {
	char *gitref = refpath(path);
	struct string_list_item *item = string_list_insert(&path_for_ref, gitref);
	struct svnref *ref = get_ref(path, listrev);
	ref->exists_at_head = 1;
	ref->is_tag = !prefixcmp(gitref, "refs/tags/");
	item->util = (void*) ref->path;
	remotef("? %s\n", gitref);
}

static void list(void) {
	int i, j, latest;
	struct strbuf buf = STRBUF_INIT;

	for_each_ref_in(refdir.buf, &load_ref_cb, NULL);

	latest = proto->get_latest();
	listrev = min(latest, listrev);

	remotef("@refs/heads/master HEAD\n");

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






static struct svn_entry *make_entry(struct svnref *r) {
	if (!r->cmts || r->cmts->rev) {
		struct svn_entry *c = xcalloc(1, sizeof(*c));
		c->next = r->cmts;
		r->cmts = c;
	}
	return r->cmts;
}

void changed_path_read(struct svnref **refs, int refnr, int ismodify, const char *path, const char *copy, int copyrev) {
	int i;
	for (i = 0; i < refnr; i++) {
		struct svnref *r = refs[i];

		/* in the corner case where the server
		 * is returning commits from the same
		 * path but the previous branch */
		if (r->cmts && r->cmts->rev && r->cmts->new_branch)
			continue;

		if (!strncmp(path, r->path, strlen(r->path))
			&& path[strlen(r->path)] == '/')
		{
			/* change to a file within this
			 * branch */
			struct svn_entry *cmt = make_entry(r);
			cmt->copy_modified = 1;

		} else if (!strncmp(r->path, path, strlen(path))
			&& (r->path[strlen(path)] == '/' || r->path[strlen(path)] == '\0')
			&& copyrev > 0)
		{
			/* copy to the branch root or to
			 * a parent */
			struct svn_entry *cmt = make_entry(r);
			struct strbuf buf = STRBUF_INIT;

			strbuf_addstr(&buf, copy);
			strbuf_addstr(&buf, r->path + strlen(path));

			cmt->copysrc = strbuf_detach(&buf, NULL);
			cmt->copyrev = (int) copyrev;
			cmt->new_branch = 1;

		} else if (!ismodify && !strcmp(path, r->path)) {
			struct svn_entry *cmt = make_entry(r);
			cmt->new_branch = 1;
		}
	}
}


struct log_request {
	char *path;
	int rev;
	char *gitref;
};

static int cmp_log_request(const void *u, const void *v) {
	const struct log_request *a = u;
	const struct log_request *b = v;
	return a->rev - b->rev;
}

static int cmts_to_fetch;
static struct log_request *log_requests;
static int log_request_nr, log_request_alloc;

static void request_log(const char *path, int rev, const char *gitref) {
	struct log_request *r;
	ALLOC_GROW(log_requests, log_request_nr+1, log_request_alloc);
	r = &log_requests[log_request_nr++];
	r->path = strdup(path);
	r->rev = rev;
	r->gitref = gitref ? strdup(gitref) : NULL;
	qsort(log_requests, log_request_nr, sizeof(log_requests[0]), &cmp_log_request);
}

static struct svnref *next_log(int *start, int *end) {
	int i;
	for (i = log_request_nr-1; i >= 0; i--) {
		struct svnref *ref;
		struct log_request *l = &log_requests[i];
		if (*end && l->rev != *end) {
			break;
		}

		ref = get_ref(l->path, l->rev);
		if (l->rev <= ref->logrev) {
			if (l->gitref) {
				string_list_insert(&ref->gitrefs, l->gitref);
			}
			free(l->path);
			memmove(l, l+1, (log_request_nr-i-1)*sizeof(*l));
			log_request_nr--;
			continue;
		}

		if (*start && ref->logrev + 1 != *start) {
			continue;
		}

		*start = ref->logrev + 1;
		*end = l->rev;

		if (l->gitref) {
			string_list_insert(&ref->gitrefs, l->gitref);
		}

		/* set this before actually doing the log so next_log
		 * can cull duplicate log requests */
		ref->logrev = l->rev;

		free(l->path);
		memmove(l, l+1, (log_request_nr-i-1)*sizeof(*l));
		log_request_nr--;

		return ref;
	}

	return NULL;
}

static void read_logs(void) {
	struct svnref **refs = NULL;
	int refnr = 0, refalloc = 0;

	if (use_progress)
		progress = start_progress("Counting commits", 0);

	for (;;) {
		int start = 0;
		int end = 0;

		while (log_request_nr > 0) {
			struct svnref *ref = next_log(&start, &end);
			if (!ref) {
				break;
			}

			ALLOC_GROW(refs, refnr+1, refalloc);
			refs[refnr++] = ref;
		}

		if (refnr == 0) {
			break;
		}

		proto->read_log(refs, refnr, start, end);
		refnr = 0;
	}

	stop_progress(&progress);
	free(refs);
}

void cmt_read(struct svnref **refs, int refnr, int rev, const char *author, const char *time, const char *msg) {
	int i;
	for (i = 0; i < refnr; i++) {
		struct svn_entry *c = refs[i]->cmts;
		if (c && !c->rev) {
			c->rev = rev;
			c->ident = strdup(svn_to_ident(author, time));
			c->msg = strdup(msg);

			if (c->new_branch) {
				set_ref_start(refs[i], c->rev);
			}
			if (c->copysrc) {
				request_log(c->copysrc, c->copyrev, NULL);
			}
			if (c->copyrev > c->rev) {
				die("copy revision newer then destination");
			}
			display_progress(progress, ++cmts_to_fetch);
		}
	}

}






static int cmp_svnref_start(const void *u, const void *v) {
	struct svnref *const *a = u;
	struct svnref *const *b = v;
	return (*a)->start - (*b)->start;
}

static struct child_process helper;
static FILE* helper_file;

static int is_helper_started(void) {
	return helper_file != NULL;
}

static void start_helper() {
	static const char *remote_svn_helper[] = {"remote-svn--helper", NULL};

	memset(&helper, 0, sizeof(helper));
	helper.argv = remote_svn_helper;
	helper.in = -1;
	helper.out = xdup(fileno(stdout));
	helper.git_cmd = 1;

	if (use_progress)
		progress = start_progress("Fetching commits", cmts_to_fetch);

	if (svndbg)
		fprintf(stderr, "starting git remote-svn--helper\n");

	if (start_command(&helper))
		die_errno("failed to launch helper");

	helper_file = xfdopen(helper.in, "wb");
	if (!helper_file)
		die_errno("failed to launch helper");

	if (svndbg >= 2)
		fprintf(helper_file, "verbose\n");
	if (gitroot.len)
		fprintf(helper_file, "chroot %d:%s\n", (int) gitroot.len, gitroot.buf);
}

static void stop_helper() {
	static const char *gc_auto[] = {"gc", "--auto", NULL};
	struct child_process ch;

	fclose(helper_file);
	helper_file = NULL;
	if (finish_command(&helper))
		die_errno("worker failed");

	stop_progress(&progress);

	if (svndbg) {
		fprintf(stderr, "finished git remote-svn--helper\n");
		fprintf(stderr, "running git gc --auto\n");
	}

	memset(&ch, 0, sizeof(ch));
	ch.argv = gc_auto;
	ch.no_stdin = 1;
	ch.no_stdout = 1;
	ch.git_cmd = 1;
	if (run_command(&ch))
		die_errno("git gc --auto failed");

	if (svndbg)
		fprintf(stderr, "finished git gc --auto\n");
}

static struct svn_entry * volatile current_commit, *last_commit;

void write_helper(struct svn_entry *c, const char *str, int len, int limitdbg) {
	if (svndbg >= 2) {
		struct strbuf buf = STRBUF_INIT;
		strbuf_addf(&buf, "H+ %d ", c->rev);
		if (!limitdbg) {
			strbuf_add(&buf, str, len);
		} else if (len > 20) {
			quote_c_style_counted(str, 20, &buf, NULL, 1);
			strbuf_addstr(&buf, "...");
		} else {
			quote_c_style_counted(str, len, &buf, NULL, 1);
		}
		strbuf_complete_line(&buf);
		fwrite(buf.buf, 1, buf.len, stderr);
		strbuf_release(&buf);
	}

	if (c == current_commit) {
		fwrite(c->update.buf, 1, c->update.len, helper_file);
		strbuf_release(&c->update);
		fwrite(str, 1, len, helper_file);
	} else {
		strbuf_add(&c->update, str, len);
	}
}

void helperf(struct svn_entry *c, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	strbuf_reset(&c->buf);
	strbuf_vaddf(&c->buf, fmt, ap);
	write_helper(c, c->buf.buf, c->buf.len, 0);
}

static int cmts_fetched;

static void do_finish_update(struct svnref *r, struct svn_entry *c) {
	c->fetched = 1;

	if (c != current_commit)
		return;

	for (c = current_commit; c && c->fetched; c = c->next) {
		fwrite(c->update.buf, 1, c->update.len, helper_file);
		strbuf_release(&c->update);
		display_progress(progress, ++cmts_fetched);
	}

	/* make sure the previous data is written before updating
	 * current_commit as write_helper can be called concurrently
	 * with this function */
	__sync_synchronize();

	current_commit = c;
	if (c == NULL) {
		last_commit = NULL;
	}
}

struct svnref **fetch;
int fetch_num, fetch_fin, fetch_alloc;
int cmts_started, next_gc_flush;

struct svn_entry* svn_start_next_update(void) {
	while (fetch_fin < fetch_num) {
		struct svnref *r = fetch[fetch_fin];

		while (r->cmts) {
			struct svn_entry *c = r->cmts;
			struct svnref *copysrc = c->copysrc
				? get_ref(c->copysrc, c->copyrev)
				: NULL;

			if (cmts_started == next_gc_flush)
				return NULL;

			cmts_started++;

			c->ref = r;
			c->prev = r->rev;
			r->rev = c->rev;
			strbuf_init(&c->update, 0);
			strbuf_init(&c->buf, 0);
			r->cmts = c->next;

			if (last_commit) {
				last_commit->next = c;
			}

			if (!current_commit) {
				current_commit = c;
			}

			last_commit = c;

			if (copysrc && !c->copy_modified) {
				helperf(c, "branch %s %d %s %d %d:%s %d:%s %d:",
						refszname(copysrc), c->copyrev,
						refszname(r), c->rev,
						(int) strlen(r->path), r->path,
						(int) strlen(c->ident), c->ident,
						(int) strlen(c->msg));

				write_helper(c, c->msg, strlen(c->msg), 1);
				do_finish_update(r, c);

			} else {
				if (copysrc) {
					helperf(c, "checkout %s %d\n", refszname(copysrc), c->copyrev);
				} else if (c->prev) {
					helperf(c, "checkout %s %d\n", refszname(r), c->prev);
				} else {
					helperf(c, "reset\n");
				}

				return c;
			}
		}

		fetch_fin++;
	}

	return NULL;
}

void svn_finish_update(struct svn_entry *c) {
	struct svnref *r = c->ref;
	helperf(c, "commit %s %d %d %d:%s %d:%s %d:",
			refszname(r),
			c->prev, c->rev,
			(int) strlen(r->path), r->path,
			(int) strlen(c->ident), c->ident,
			(int) strlen(c->msg));

	write_helper(c, c->msg, strlen(c->msg), 1);
	do_finish_update(r, c);
}

static void fetch_updates(void) {
	int i;

	for (i = 0; i < refs.nr; i++) {
		struct svnref *r;
		for (r = refs.items[i].util; r != NULL; r = r->next) {
			if (r->cmts) {
				ALLOC_GROW(fetch, fetch_num+1, fetch_alloc);
				fetch[fetch_num++] = r;
			}
		}
	}

	/* sort by start so that copysrcs are requested first */
	qsort(fetch, fetch_num, sizeof(fetch[0]), &cmp_svnref_start);

	/* Farm the pending requests out to a subprocess so that we can
	 * run git gc --auto after each chunk. We have to this as after
	 * calling gc --auto, we can't rely on any locally loaded
	 * objects being valid.
	 */

	while (next_gc_flush < cmts_to_fetch) {
		int cmts = min(gcperiod, cmts_to_fetch - next_gc_flush);
		next_gc_flush += cmts;

		/* The GC can take awhile and the protocol connections
		 * expire. So we explicitely close and then reopen them
		 * later */
		if (is_helper_started()) {
			proto->disconnect();
			stop_helper();
			do_connect(0);
		}

		start_helper();
		proto->read_updates(cmts);
	}

	if (!is_helper_started())
		start_helper();

	if (current_commit) {
		die("internal: haven't fetched all commits");
	}

	for (i = 0; i < refs.nr; i++) {
		struct svnref *r;
		for (r = refs.items[i].util; r != NULL; r = r->next) {
			if (r->gitrefs.nr) {
				int i;
				for (i = 0; i < r->gitrefs.nr; i++) {
					const char *gitref = r->gitrefs.items[i].string;
					fprintf(helper_file, "report %s %d:%s\n",
							refszname(r),
							(int) strlen(gitref),
							gitref);
				}
			}

			if (!r->exists_at_head)
				r->logrev = r->rev;

			fprintf(helper_file, "havelog %s %d %d\n", refszname(r), r->rev, r->logrev);
		}
	}

	stop_helper();
}






static struct svnref *push_ref;

static void push_paths(const char **path, const char **svn) {
	static struct strbuf sbuf = STRBUF_INIT, gbuf = STRBUF_INIT;
	strbuf_reset(&gbuf);
	strbuf_addstr(&gbuf, *path);
	clean_svn_path(&gbuf);
	*path = gbuf.buf;

	strbuf_reset(&sbuf);
	strbuf_addstr(&sbuf, push_ref->path);
	strbuf_addstr(&sbuf, gbuf.buf + gitroot.len);
	*svn = sbuf.buf;
}

static void send_file(const char *path, const char *svnpath, const unsigned char *sha1) {
	struct strbuf diff = STRBUF_INIT;
	struct strbuf buf = STRBUF_INIT;
	char *data;
	unsigned long sz;
	enum object_type type;
	struct cache_entry* ce;
	unsigned char nsha1[20];
	int create;

	data = read_sha1_file(sha1, &type, &sz);
	if (type != OBJ_BLOB)
		die("unexpected object type for %s", sha1_to_hex(sha1));

	if (svn_eol != EOL_UNSET && convert_to_working_tree(path+1, data, sz, &buf)) {
		free(data);
		data = strbuf_detach(&buf, &sz);

		if (write_sha1_file(data, sz, "blob", nsha1)) {
			die_errno("blob write");
		}

		sha1 = nsha1;
	}

	create = index_name_exists(&svn_index, path+gitroot.len+1, strlen(path+gitroot.len+1), 0) == NULL;

	ce = make_cache_entry(create_ce_mode(0644), sha1, path+gitroot.len+1, 0, 0);
	if (!ce) die("make_cache_entry failed for '%s'", path+gitroot.len+1);
	add_index_entry(&svn_index, ce, ADD_CACHE_OK_TO_ADD | ADD_CACHE_OK_TO_REPLACE);

	create_svndiff(&diff, data, sz);
	proto->send_file(svnpath, &diff, create);

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
	const char *svnpath;

	push_paths(&path, &svnpath);

	if (svndbg) fprintf(stderr, "change mode %x/%x sha1 %.10s/%.10s path %s\n",
			omode, nmode, sha1_to_hex(osha1), sha1_to_hex(nsha1), path);

	if (prefixcmp(path, gitroot.buf))
		die("change called for '%s' outside gitroot '%s'", path, gitroot.buf);

	if (S_ISREG(nmode) && prefixcmp(strrchr(path, '/'), "/.git")) {
		send_file(path, svnpath, nsha1);
	}

	/* TODO make this actually use diffcore */
	diff_change(op, omode, nmode, osha1, nsha1, osha1_valid, nsha1_valid, path+1, odsubmodule, ndsubmodule);
}

static void addremove(struct diff_options *op,
		int addrm,
		unsigned mode,
		const unsigned char *sha1,
		int sha1_valid,
		const char *path,
		unsigned dsubmodule)
{
	const char *svnpath;
	push_paths(&path, &svnpath);

	if (svndbg) fprintf(stderr, "addrm %c mode %x sha1 %.10s path %s\n",
			addrm, mode, sha1_to_hex(sha1), path);

	if (prefixcmp(path, gitroot.buf))
		die("addremove called for '%s' outside gitroot '%s'", path, gitroot.buf);

	if (addrm == '-' && !remove_path_from_index(&svn_index, path+gitroot.len+1)) {
		/* only push the delete if we have something to remove
		 * in svn */
		proto->delete(svnpath);

	} else if (addrm == '+' && S_ISDIR(mode) && strcmp(path, gitroot.buf)) {
		/* the creation of the root of the branch in svn is
		 * handled when starting the push */
		proto->mkdir(svnpath);

	} else if (addrm == '+' && S_ISREG(mode) && prefixcmp(strrchr(path, '/'), "/.git")) {
		/* files beginning with .git eg .gitempty,
		 * .gitattributes, etc are filtered from svn
		 *
		 * gitpath can change where a file already exists in svn
		 * from a different gitpath, but git sees it as an add
		 */
		send_file(path, svnpath, sha1);
	}

	/* TODO use diffcore to track renames */
	diff_addremove(op, addrm, mode, sha1, sha1_valid, path+1, dsubmodule);
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

#define PUSH_SECOND_PARENT 1
#define PUSH_FIRST_PARENT_TAG 2
#define PUSH_FIRST_PARENT_NEW 4
#define PUSH_FIRST_PARENT 8
#define PUSH_NEW_COMMIT_MASK 15
#define PUSH_IN_SVN 16
#define PUSH_SVN_CMT 32
#define PUSH_MASK 63
#define MI_FIRST_PARENT 0x40
#define MI_MERGED_PARENT 0x80
#define MI_IN_SVN 0x100
#define MI_SVN_CMT 0x200
#define MI_MASK 0x3C0

static void insert_by_date(struct commit *c, struct commit_list **cmts, int mask) {
	if (parse_commit(c))
		die("invalid commit %s", sha1_to_hex(c->object.sha1));
	if (!(c->object.flags & mask))
		commit_list_insert_by_date(c, cmts);
}

static void insert_svn_commit(struct commit *svn, struct commit_list **cmts, int mask, int svncmt, int insvn) {
	if (svn) {
		struct commit *cmt = svn_commit(svn);

		insert_by_date(svn, cmts, mask);
		insert_by_date(cmt, cmts, mask);

		svn->object.flags |= svncmt;

		if (!(cmt->object.flags & insvn)) {
			cmt->util = svn;
			cmt->object.flags |= insvn;
		}
	}
}

static void compute_mergeinfo(struct commit *c, struct mergeinfo* mi) {
	/* we want to find all parents of c recursively that are not
	 * included in the first parent copy path */
	struct commit_list *cmts = NULL;
	struct commit_list *pp = c->parents;
	int i, cmts_to_find = 0;

	/* push the initial parents to find */
	insert_by_date(pp->item, &cmts, MI_MASK);
	pp->item->object.flags |= MI_FIRST_PARENT;

	for (pp = pp->next; pp != NULL; pp = pp->next) {
		struct commit *p = pp->item;
		if (!(p->object.flags & MI_MERGED_PARENT)) {
			cmts_to_find++;
			insert_by_date(pp->item, &cmts, MI_MASK);
			pp->item->object.flags |= MI_MERGED_PARENT;
		}
	}

	/* push the svn heads */
	for (i = 0; i < refs.nr; i++) {
		struct svnref *r;
		for (r = refs.items[i].util; r != NULL; r = r->next) {
			if (!r->is_tag) {
				insert_svn_commit(r->svn, &cmts, MI_MASK, MI_SVN_CMT, MI_IN_SVN);
			}
		}
	}

	while (cmts && cmts_to_find) {
		c = pop_commit(&cmts);

		if (c->object.flags & MI_SVN_CMT) {
			insert_svn_commit(svn_parent(c), &cmts, MI_MASK, MI_SVN_CMT, MI_IN_SVN);

		} else if (c->object.flags & MI_FIRST_PARENT) {
			pp = c->parents;
			if (pp) {
				insert_by_date(pp->item, &cmts, MI_MASK);
				pp->item->object.flags |= MI_FIRST_PARENT;
				pp = pp->next;
			}

			if (c->object.flags & MI_MERGED_PARENT) {
				cmts_to_find--;
			}

		} else if (c->object.flags & MI_MERGED_PARENT) {
			if (c->object.flags & MI_IN_SVN) {
				struct commit *svn = c->util;
				const char *path = get_svn_path(svn);
				int rev = get_svn_revision(svn);
				add_svn_mergeinfo(mi, path, rev, rev);
			}

			cmts_to_find--;

			for (pp = c->parents; pp != NULL; pp = pp->next) {
				struct commit *p = pp->item;
				if (!(p->object.flags & MI_MERGED_PARENT)) {
					cmts_to_find++;
					insert_by_date(pp->item, &cmts, MI_MASK);
					pp->item->object.flags |= MI_MERGED_PARENT;
				}
			}
		}
	}

	clear_object_flags(MI_MASK);
	free_commit_list(cmts);
}

static int push_commit(struct svnref *r, int type, struct object *obj, const char *specdst, int use_cmt_msg) {
	static struct strbuf buf = STRBUF_INIT;

	const char *ident, *log = push_message(obj, use_cmt_msg);
	int rev, copyrev = 0;
	const char *copypath = NULL;
	struct credential *cred = push_credential(obj, use_cmt_msg);
	unsigned char sha1[20];
	struct mergeinfo *mergeinfo;
	struct commit *svnbase, *cmt;
	struct diff_options op;
	struct pathspec_item ps_item = {gitroot.buf+1, gitroot.len-1, 0};
	struct pathspec gitpathspec = {NULL, 1, 0, 0, 0, &ps_item};

	cmt = (struct commit*) deref_tag(obj, NULL, 0);
	if (parse_commit(cmt) || cmt->object.type != OBJ_COMMIT)
		die("invalid push object %s", sha1_to_hex(obj->sha1));

	proto->change_user(cred);

	if (!use_cmt_msg || obj->type == OBJ_TAG) {
		svnbase = cmt->util;
		if (!svnbase) die("internal: no base");

		mergeinfo = proto->get_mergeinfo(
				get_svn_path(svnbase),
				get_svn_revision(svnbase));

	} else if (cmt->parents) {
		struct commit_list *pp = cmt->parents;

		svnbase = pp->item->util;
		if (!svnbase) die("internal: no base");

		mergeinfo = proto->get_mergeinfo(
				get_svn_path(svnbase),
				get_svn_revision(svnbase));

		if (pp && pp->next) {
			compute_mergeinfo(cmt, mergeinfo);
		}

	} else {
		svnbase = NULL;
		mergeinfo = parse_svn_mergeinfo("");
	}

	if (type == SVN_ADD || type == SVN_REPLACE) {
		copypath = get_svn_path(svnbase);
		copyrev = get_svn_revision(svnbase);
	}

	/* need to checkout the git commit in order to get the
	 * .gitattributes files used for eol conversion
	 */
	svn_checkout_index(&the_index, cmt);
	svn_checkout_index(&svn_index, svnbase);

	proto->start_commit(type, log, r->path, max(r->rev, copyrev) + 1, copypath, copyrev);

	/* we always send mergeinfo so that at least some property
	 * changes on the path so svn log picks up the commit for this
	 * path */
	proto->set_mergeinfo(r->path, mergeinfo);

	diff_setup(&op);
	op.output_format = DIFF_FORMAT_NO_OUTPUT;
	op.change = &change;
	op.add_remove = &addremove;
	DIFF_OPT_SET(&op, RECURSIVE);
	DIFF_OPT_SET(&op, IGNORE_SUBMODULES);
	DIFF_OPT_SET(&op, TREE_IN_RECURSIVE);

	if (gitroot.len)
		op.pathspec = gitpathspec;

	push_ref = r;

	if (svnbase) {
		if (diff_tree_sha1(svn_commit(svnbase)->object.sha1, obj->sha1, "", &op))
			die("diff tree failed");
	} else {
		if (diff_root_tree_sha1(obj->sha1, "", &op))
			die("diff tree failed");
	}

	diffcore_std(&op);
	diff_flush(&op);

	strbuf_reset(&buf);
	rev = proto->finish_commit(&buf);
	ident = svn_to_ident(cred->username, buf.buf);

	if (rev <= 0) {
		/* TODO get the full error message */
		remotef("error %s commit failed\n", specdst);
		return -1;
	}

	/* If we find any intermediate commits, we complain. They will
	 * be picked up the next time the user does a pull.
	 */
	if (type == SVN_MODIFY && r->logrev+1 < rev
		&& proto->has_change(r->path, r->logrev+1, rev-1))
	{
		remotef("error %s non-fast forward\n", specdst);
		return -1;
	}

	/* update the svn ref */

	if (!svn_index.cache_tree)
		svn_index.cache_tree = cache_tree();
	if (cache_tree_update(svn_index.cache_tree, svn_index.cache, svn_index.cache_nr, 0))
		die("failed to update cache tree");

	if (write_svn_commit(r->svn, cmt, svn_index.cache_tree->sha1,
				ident, r->path, rev, sha1)) {
		die("failed to write svn commit");
	}

	if (!r->start || type == SVN_ADD || type == SVN_REPLACE)
		set_ref_start(r, rev);

	update_ref("remote-svn", refname(r), sha1,
			r->svn ? r->svn->object.sha1 : null_sha1,
			0, DIE_ON_ERR);

	r->exists_at_head = 1;
	r->rev = rev;
	r->logrev = rev;
	r->svn = lookup_commit(sha1);

	/* update the svn tag ref */
	strbuf_reset(&buf);
	strbuf_addstr(&buf, refname(r));
	strbuf_addstr(&buf, ".tag");

	if (obj->type == OBJ_TAG) {
		if (read_ref(buf.buf, sha1)) {
			hashclr(sha1);
		}
		update_ref("remote-svn", buf.buf, obj->sha1, sha1, 0, DIE_ON_ERR);

	} else if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}

	/* remove the .log ref as it's no longer valid */
	strbuf_reset(&buf);
	strbuf_addstr(&buf, refname(r));
	strbuf_addstr(&buf, ".log");

	if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}

	free_svn_mergeinfo(mergeinfo);

	trypause();
	return 0;
}

/* When pushing commits we have the following goals:
 * a) svn log should look the same as git log --first-parent
 * b) svn log --use-merge-history should look the same as git log
 * c) commits shouldn't be pushed more than once
 * d) minimise replaces on existing svn branches
 *	- commits which are reachable from more than one git head are
 *	prioritised to the existing svn branches
 *	- commits are prioritised away from branches that will require a
 *	replace
 * e) avoid creating a branch from a tag
 *
 * In order to do this we trace from the new heads back and categorize
 * each commit with one of the following priorities.
 *
 * cmt->util holds a svn_push for PUSH_FIRST_PARENT*, and the svn commit for
 * IN_SVN
 */

struct svn_push {
	struct svn_push *next;
	struct object *obj;
	struct svnref *ref;
	struct refspec *spec;
	unsigned int replace : 1;
};

static int is_first_parent(struct commit *child, struct commit *parent) {
	if (!child || !parent)
		return 0;

	if (parse_commit(parent))
		die("invalid commit %s", sha1_to_hex(parent->object.sha1));

	for (;;) {
		if (parse_commit(child))
			die("invalid commit %s", sha1_to_hex(child->object.sha1));

		if (child->date < parent->date)
			return 0;

		if (child == parent)
			return 1;

		if (!child->parents)
			return 0;

		child = child->parents->item;
	}
}

/* list of commits found during the push traversal sorted by date then
 * flags (newest svncmt first)
 */
static struct commit_list *push_all;
static struct svn_push *push_heads;
static int push_cmts_to_find;

static void insert_commit(struct commit *c, int flag, void *util) {
	int flags = c->object.flags;

	if ((flags & PUSH_MASK) < flag)
		c->util = util;

	if ((flags & PUSH_NEW_COMMIT_MASK) == 0)
		push_cmts_to_find++;

	insert_by_date(c, &push_all, PUSH_MASK);
	c->object.flags |= flag;
}

static struct refspec **pushv;
static size_t pushn, pusha;

static void push(void) {
	int i;
	struct commit_list *cmts = NULL;
	int cmts_to_push = 0;
	struct svn_push *push;
	struct refspec twig_spec;
	struct svn_push *twig, twig_push;

	if (twig_path.len) {
		memset(&twig_spec, 0, sizeof(twig_spec));
		twig_spec.force = 1;
		twig_spec.dst = twig_path.buf;

		memset(&twig_push, 0, sizeof(twig_push));
		twig_push.ref = get_ref(twig_path.buf, INT_MAX);
		twig_push.spec = &twig_spec;

		twig = &twig_push;

		if (proto->isdir(twig_path.buf, listrev)) {
			twig_push.ref->exists_at_head = 1;
		}
	}

	/* push all svn heads */
	for (i = 0; i < refs.nr; i++) {
		struct svnref *r;
		for (r = refs.items[i].util; r != NULL; r = r->next) {
			if (!r->is_tag) {
				insert_svn_commit(r->svn, &push_all, PUSH_MASK, PUSH_SVN_CMT, PUSH_IN_SVN);
			}
		}
	}

	/* push all the new heads */
	for (i = 0; i < pushn; i++) {
		struct refspec *spec = pushv[i];
		struct object *obj = NULL;
		struct commit *cmt = NULL;
		struct svn_push *push;
		int flag;

		if (*spec->src) {
			unsigned char sha1[20];
			if (read_ref(spec->src, sha1) && get_sha1_hex(spec->src, sha1))
				die("invalid ref %s", spec->src);

			obj = parse_object(sha1);
			cmt = (struct commit*) deref_tag(obj, spec->src, 0);

			if (cmt->object.type != OBJ_COMMIT)
				die("invalid ref %s", spec->src);
		}

		push = xcalloc(1, sizeof(*push));
		push->ref = get_push_ref(spec);
		push->spec = spec;
		push->obj = obj;

		if (!push->ref) {
			remotef("error %s not mapped\n", spec->dst);
			return;
		}

		if (cmt) {
			struct svnref *r = push->ref;

			if (!prefixcmp(spec->dst, "refs/tags/")) {
				r->is_tag = 1;
				push->replace = 1;
				flag = PUSH_FIRST_PARENT_TAG;

			} else if (!r->svn || !r->exists_at_head) {
				flag = PUSH_FIRST_PARENT_NEW;

			} else if (is_first_parent(cmt, svn_commit(r->svn))) {
				flag = PUSH_FIRST_PARENT;

			} else if (!spec->force) {
				remotef("error %s non-fast forward\n", spec->dst);
				return;

			} else {
				flag = PUSH_FIRST_PARENT_NEW;
			}

			insert_commit(cmt, flag, push);
		}

		push->next = push_heads;
		push_heads = push;
	}

	/* Date based search the tree from the svn and new heads marking
	 * commits with whether they need to be added to svn and if so
	 * onto which branch.
	 */
	while (push_all && push_cmts_to_find > 0) {
		struct commit *c = pop_commit(&push_all);
		int flags = c->object.flags;

		if (flags & PUSH_SVN_CMT) {
			insert_svn_commit(svn_parent(c), &push_all, PUSH_MASK, PUSH_SVN_CMT, PUSH_IN_SVN);

		} else if (flags & PUSH_NEW_COMMIT_MASK) {
			if ((flags & PUSH_IN_SVN) == 0) {
				struct commit_list *pp = c->parents;
				if (pp) {
					insert_commit(pp->item, flags, c->util);
					pp = pp->next;
				}
				while (twig && pp) {
					insert_commit(pp->item, PUSH_SECOND_PARENT, twig);
					pp = pp->next;
				}
				cmts_to_push++;
			}

			push_cmts_to_find--;
		}
	}

	free_commit_list(push_all);

	if (use_progress)
		progress = start_progress("Pushing commits", cmts_to_push);

	/* We now push all the commits. We maintain a stack in cmts and
	 * do a depth first search so that parent commits are processed
	 * first. We can't build a list from the priority tagging as the
	 * date is unreliable (parents can be newer).
	 */
	for (push = push_heads; push != NULL; push = push->next) {
		struct object *obj = deref_tag_noverify(push->obj);
		if (obj && obj->type == OBJ_COMMIT) {
			commit_list_insert((struct commit*) obj, &cmts);
		}
	}

	while (cmts) {
		struct commit *c = cmts->item;
		struct svn_push *p;
		struct svnref *r;
		struct commit_list *pp;
		int type;

		if ((c->object.flags & PUSH_IN_SVN) || (c->object.flags & PUSH_NEW_COMMIT_MASK) == 0) {
			pop_commit(&cmts);
			continue;
		}

		for (pp = c->parents; pp != NULL; pp = pp->next) {
			int flags = pp->item->object.flags;
			if ((flags & PUSH_IN_SVN) == 0 && (flags & PUSH_NEW_COMMIT_MASK)) {
				commit_list_insert(pp->item, &cmts);
			}
		}

		/* We had at least one parent not in svn. We will come
		 * back to this commit later. */
		if (cmts->item != c) {
			continue;
		}

		p = c->util;
		r = p->ref;
		pop_commit(&cmts);

		if (!r->exists_at_head) {
			type = SVN_ADD;
		} else if (p->replace || !c->parents || c->parents->item != svn_commit(r->svn)) {
			type = SVN_REPLACE;
		} else {
			type = SVN_MODIFY;
		}

		if (type == SVN_MODIFY && r->logrev < listrev) {
			if (proto->has_change(r->path, r->logrev+1, listrev)) {
				type = SVN_REPLACE;
			}
		}

		if (!p->spec->force && type == SVN_REPLACE) {
			remotef("error %s non-fast forward\n", p->spec->dst);
			return;
		}

		/* we don't need to add the root on the initial commit */
		if (!*r->path && type == SVN_ADD) {
			type = SVN_MODIFY;
		}

		if (push_commit(r, type, &c->object, p->spec->dst, 1))
			return;

		c->object.flags |= PUSH_IN_SVN;
		c->util = r->svn;
		p->replace = 0;
	}

	/* Finally push through branch changes which weren't covered by
	 * pushing new commits: branch deletion and branch creation
	 * without a new commit (or the commit being pushed to a
	 * different branch), pushing a tag, etc.
	 */
	for (push = push_heads; push != NULL; push = push->next) {
		struct svnref *r = push->ref;
		struct refspec *spec = push->spec;

		if (!push->obj) {
			struct strbuf buf = STRBUF_INIT;
			unsigned char sha1[20];

			proto->change_user(&defcred);
			proto->start_commit(SVN_DELETE, empty_log_message, r->path, r->logrev+1, NULL, 0);

			if (proto->finish_commit(NULL) <= 0) {
				remotef("error %s commit failed\n", spec->dst);
				return;
			}

			strbuf_addstr(&buf, refname(r));
			strbuf_addstr(&buf, ".log");

			if (!read_ref(buf.buf, sha1)) {
				delete_ref(buf.buf, sha1, 0);
			}

			strbuf_release(&buf);
			r->exists_at_head = 0;

		} else if (!r->svn || &svn_commit(r->svn)->object != push->obj) {
			int type, use_cmt_msg = (push->obj->type == OBJ_TAG);

			if (!r->exists_at_head) {
				type = SVN_ADD;
			} else if (push->obj->type == OBJ_TAG && !push->replace) {
				type = SVN_MODIFY;
			} else {
				type = SVN_REPLACE;
			}

			if (!spec->force && type == SVN_REPLACE) {
				remotef("error %s non-fast forward\n", spec->dst);
				return;
			}

			if (push_commit(r, type, push->obj, spec->dst, use_cmt_msg))
				return;
		}

		remotef("ok %s\n", spec->dst);
	}
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
	if (log_request_nr && !strcmp(cmd, "")) {
		read_logs();
		fetch_updates();
		remotef("\n");
		return 1;

	} else if (!strcmp(cmd, "fetch")) {
		char *ref, *path;

		next_arg(arg, &arg); /* sha1 */
		ref = next_arg(arg, &arg);
		if (!strcmp(ref, "HEAD"))
			ref = "refs/heads/master";

		path = map_lookup(&path_for_ref, ref);
		if (!path)
			die("unexpected fetch ref %s", ref);

		request_log(path, listrev, ref);

	} else if (pushn && !strcmp(cmd, "")) {
		int i;

		for (i = 0; i < refmap_nr; i++) {
			char *tmp = refmap[i].src;
			refmap[i].src = refmap[i].dst;
			refmap[i].dst = tmp;
		}

		push();
		remotef("\n");
		return 1;

	} else if (!strcmp(cmd, "push")) {
		const char *ref = next_arg(arg, &arg);
		struct refspec *spec = parse_push_refspec(1, &ref);

		ALLOC_GROW(pushv, pushn+1, pusha);
		pushv[pushn++] = spec;

	} else if (!strcmp(cmd, "capabilities")) {
		remotef("option\n");
		remotef("fetch\n");
		remotef("*fetch-unknown\n");
		remotef("push\n");
		remotef("\n");

	} else if (!strcmp(cmd, "option")) {
		char *opt = next_arg(arg, &arg);

		if (!strcmp(opt, "verbosity")) {
			verbose = strtol(arg, &arg, 10);
			remotef("ok\n");
		} else if (!strcmp(opt, "progress")) {
			use_progress = !strcmp(next_arg(arg, &arg), "true");
			remotef("ok\n");
		} else {
			remotef("unsupported\n");
		}

		if (use_progress && verbose <= 1) {
			svndbg = 0;
		} else {
			svndbg = verbose;
		}

	} else if (!strcmp(cmd, "list")) {
		int ispush = !strcmp(next_arg(arg, &arg), "for-push");
		do_connect(ispush);
		list();
		remotef("\n");

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
		/* parse_fetch_refspec doesn't like svn paths as they
		 * can contain spaces etc, so we create a cleaned
		 * version of the source, pass that to
		 * parse_fetch_refspec and then swap that out for the
		 * svn path afterwards */
		int i;
		struct strbuf buf = STRBUF_INIT;
		char **gitrefs = malloc(refmap_nr * sizeof(*gitrefs));
		for (i = 0; i < refmap_nr; i++) {
			char *path = refmap_str[i];
			while (*path && *path != ':') {
				int ch = *(path++);
				strbuf_addch(&buf, (ch != '*' && bad_ref_char(ch)) ? '_' : ch);
			}
			strbuf_addstr(&buf, path);
			*path = '\0'; /* update refmap_str to only have the left side */
			gitrefs[i] = strbuf_detach(&buf, NULL);
		}

		refmap = parse_fetch_refspec(refmap_nr, (const char**) gitrefs);

		strbuf_release(&buf);
		for (i = 0; i < refmap_nr; i++) {
			free(refmap[i].src);
			free(gitrefs[i]);
			refmap[i].src = refmap_str[i];
		}
		free(gitrefs);
	} else {
		refmap = xcalloc(1, sizeof(*refmap));
		refmap->src = (char*) "";
		refmap->dst = (char*) "refs/heads/master";
		refmap_nr = 1;
	}

	parse_authors();

	while (strbuf_getline(&buf, stdin, '\n') != EOF) {
		char *cmd, *arg = buf.buf;
		if (svndbg >= 2)
			fprintf(stderr, "R- %s\n", buf.buf);
		cmd = next_arg(arg, &arg);
		if (command(cmd, arg))
			break;
		fflush(stdout);
	}

	if (proto)
		proto->disconnect();

	return 0;
}

