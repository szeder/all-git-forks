#include "remote-svn.h"
#include "exec_cmd.h"
#include "refs.h"

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) ((a) < (b) ? (b) : (a))
#endif

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
static struct refspec *refmap;
static const char **refmap_str;
static int refmap_nr, refmap_alloc;
static struct string_list refs = STRING_LIST_INIT_DUP;
static struct string_list path_for_ref = STRING_LIST_INIT_NODUP;

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

	unsigned int exists_at_head : 1;
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

static char* next_arg(char *arg, char **endp) {
	char *p;
	arg += strspn(arg, " ");
	p = arg + strcspn(arg, " \n");
	if (*p) *(p++) = '\0';
	*endp = p;
	return arg;
}

static int command(char *cmd, char *arg) {
	if (!strcmp(cmd, "capabilities")) {
		printf("option\n");
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

