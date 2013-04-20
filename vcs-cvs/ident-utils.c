#include <stdio.h>

#include "vcs-cvs/ident-utils.h"
#include "hash.h"
#include "git-compat-util.h"
#include "strbuf.h"
#include "run-command.h"
#include "cache.h"

#define HASH_TABLE_INIT { 0, 0, NULL }
static struct hash_table cvs_authors_hash = HASH_TABLE_INIT;
static int cvs_authors_hash_modified = 0;

struct cvs_author {
	char *userid;
	char *ident;
};
static char *cvs_authors_lookup(const char *userid);
static void cvs_authors_add(char *userid, char *ident);

static unsigned int hash_userid(const char *userid)
{
	//unsigned int hash = 0x123;
	unsigned int hash = 0x12375903;

	while (*userid) {
		unsigned char c = *userid++;
		//c = icase_hash(c);
		hash = hash*101 + c;
	}
	return hash;
}

static int run_author_convert_hook(const char *userid, struct strbuf *author_ident)
{
	struct child_process proc;
	const char *argv[3];
	int code;

	argv[0] = find_hook("cvs-author-convert");
	if (!argv[0])
		return 0;

	argv[1] = userid;
	argv[2] = NULL;

	memset(&proc, 0, sizeof(proc));
	proc.argv = argv;
	proc.out = -1;

	code = start_command(&proc);
	if (code)
		return code;

	strbuf_getwholeline_fd(author_ident, proc.out, '\n');
	strbuf_trim(author_ident);
	close(proc.out);
	return finish_command(&proc);
}

static char *author_convert_via_hook(const char *userid)
{
	struct strbuf author_ident = STRBUF_INIT;

	if (run_author_convert_hook(userid, &author_ident))
		return NULL;

	if (!author_ident.len) {
		strbuf_addf(&author_ident, "%s <unknown>", userid);
		return strbuf_detach(&author_ident, NULL);
	}

	/*
	 * TODO: proper verify
	 */
	const char *lt;
	const char *gt;

	lt = index(author_ident.buf, '<');
	gt = index(author_ident.buf, '>');

	if (!lt && !gt)
		strbuf_addstr(&author_ident, " <unknown>");

	return strbuf_detach(&author_ident, NULL);
}

const char *author_convert(const char *userid)
{
	char *ident;

	ident = cvs_authors_lookup(userid);
	if (ident)
		return ident;

	ident = author_convert_via_hook(userid);
	if (ident)
		cvs_authors_add(xstrdup(userid), ident);

	return ident;
}

static char *cvs_authors_lookup(const char *userid)
{
	struct cvs_author *auth;

	auth = lookup_hash(hash_userid(userid), &cvs_authors_hash);
	if (auth)
		return auth->ident;

	return NULL;
}

static void cvs_authors_add(char *userid, char *ident)
{
	struct cvs_author *auth;
	struct cvs_author **ppauth;

	auth = xmalloc(sizeof(*auth));
	auth->userid = userid;
	auth->ident = ident;

	ppauth = (struct cvs_author **)insert_hash(hash_userid(userid), auth, &cvs_authors_hash);
	if (ppauth) {
		if (strcmp((*ppauth)->userid, auth->userid))
			error("cvs-authors userid hash colision %s %s", (*ppauth)->userid, auth->userid);
		else
			error("cvs-authors userid dup %s", auth->userid);
	}
	cvs_authors_hash_modified = 1;
}

void cvs_authors_load()
{
	struct strbuf line = STRBUF_INIT;
	char *p;
	FILE *fd;

	if (!is_empty_hash(&cvs_authors_hash))
		return;

	fd = fopen(git_path("cvs-authors"), "r");
	if (!fd)
		return;

	while (!strbuf_getline(&line, fd, '\n')) {
		p = strchr(line.buf, '=');
		if (!p) {
			warning("bad formatted cvs-authors line: %s", line.buf);
			continue;
		}
		*p++ = '\0';

		cvs_authors_add(xstrdup(line.buf), xstrdup(p));
	}

	fclose(fd);
	strbuf_release(&line);
	cvs_authors_hash_modified = 0;
}

static int cvs_author_item_store(void *ptr, void *data)
{
	struct cvs_author *auth = ptr;
	FILE *fd = data;

	fprintf(fd, "%s=%s\n", auth->userid, auth->ident);
	free(auth->userid);
	free(auth->ident);
	free(auth);
	return 0;
}

void cvs_authors_store()
{
	if (!cvs_authors_hash_modified)
		return;

	FILE *fd = fopen(git_path("cvs-authors"), "w");
	if (!fd)
		return;

	for_each_hash(&cvs_authors_hash, cvs_author_item_store, fd);
	fclose(fd);
	free_hash(&cvs_authors_hash);
}
