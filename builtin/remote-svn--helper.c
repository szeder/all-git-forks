#include "cache.h"
#include "builtin.h"
#include "svn.h"
#include "quote.h"
#include "refs.h"
#include "cache-tree.h"
#include <openssl/md5.h>

static struct index_state svn_index;
static int svn_eol = EOL_UNSET;
static int verbose;

static void trypause(void) {
	static int env = -1;

	if (env == -1) {
		env = getenv("GIT_REMOTE_SVN_HELPER_PAUSE") != NULL;
	}

	if (env) {
		struct stat st;
		int fd = open("remote-svn-helper-pause", O_CREAT|O_RDWR, 0666);
		close(fd);
		while (!stat("remote-svn-helper-pause", &st)) {
			sleep(1);
		}
	}
}

static const char* cmt_to_hex(struct commit* c) {
	return sha1_to_hex(c ? c->object.sha1 : null_sha1);
}

static const unsigned char *idx_tree(struct index_state *idx) {
	if (!idx->cache_tree)
		idx->cache_tree = cache_tree();
	if (cache_tree_update(idx->cache_tree, idx->cache, idx->cache_nr, 0))
		die("failed to update cache tree");
	return idx->cache_tree->sha1;
}

static const unsigned char *cmt_tree(struct commit *c) {
	if (parse_commit(c))
		die("invalid commit %s", cmt_to_hex(c));
	return c->tree->object.sha1;
}

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
	}

	return git_default_config(key, value, dummy);
}

static struct strbuf indbg = STRBUF_INIT;

static void read_atom(struct strbuf* buf) {
	strbuf_reset(buf);
	for (;;) {
		int ch = getchar();
		if (ch == EOF || (isspace(ch) && buf->len)) {
			break;
		} else if (!isspace(ch)) {
			strbuf_addch(buf, ch);
		}
	}

	if (verbose) {
		strbuf_addch(&indbg, ' ');
		strbuf_addstr(&indbg, buf->buf);
	}
}

static int read_number(void) {
	int num = 0;
	int haveval = 0;

	for (;;) {
		int ch = getchar();
		if (ch == EOF || (haveval && (ch < '0' || ch > '9')))
			break;

		if ('0' <= ch && ch <= '9') {
			num = (num * 10) + (ch - '0');
			haveval = 1;
		} else if (!isspace(ch)) {
			die("invalid value");
		}
	}

	if (verbose)
		strbuf_addf(&indbg, " %d", num);

	return num;
}

static void read_string(struct strbuf *s) {
	int len = read_number();
	strbuf_reset(s);
	if (strbuf_fread(s, len, stdin) != len)
		die_errno("read");

	if (verbose) {
		static struct strbuf qbuf = STRBUF_INIT;
		strbuf_addch(&indbg, ':');
		strbuf_reset(&qbuf);

		if (s->len > 20) {
			quote_c_style_counted(s->buf, 20, &qbuf, NULL, 1);
			strbuf_add(&indbg, qbuf.buf, qbuf.len);
			strbuf_addstr(&indbg, "...");
		} else {
			quote_c_style_counted(s->buf, s->len, &qbuf, NULL, 1);
			strbuf_add(&indbg, qbuf.buf, qbuf.len);
		}
	}
}

static void read_command(void) {
	if (verbose) {
		fprintf(stderr, "H-%s\n", indbg.buf);
	}
	strbuf_reset(&indbg);
}

static void add_dir(const char *name) {
	struct strbuf buf = STRBUF_INIT;
	struct cache_entry* ce;
	unsigned char sha1[20];
	char *p;

	if (*name == '/')
		name++;

	p = strrchr(name, '/');
	/* add ./.gitempty */
	if (write_sha1_file(NULL, 0, "blob", sha1))
		die("failed to write .gitempty object");
	strbuf_addstr(&buf, name);
	strbuf_addstr(&buf, "/.gitempty");
	ce = make_cache_entry(create_ce_mode(0644), sha1, buf.buf, 0, 0);
	add_index_entry(&the_index, ce, ADD_CACHE_OK_TO_ADD);

	/* remove ../.gitempty */
	if (p) {
		strbuf_reset(&buf);
		strbuf_add(&buf, name, p - name);
		strbuf_addstr(&buf, "/.gitempty");
		remove_file_from_index(&the_index, buf.buf);
	}

	strbuf_release(&buf);
}

static void checkmd5(const char *hash, const void *data, size_t sz) {
	unsigned char h1[16], h2[16];
	MD5_CTX ctx;

	if (get_md5_hex(hash, h1))
		die("invalid md5 hash %s", hash);

	MD5_Init(&ctx);
	MD5_Update(&ctx, data, sz);
	MD5_Final(h2, &ctx);

	if (memcmp(h1, h2, sizeof(h2)))
		die("hash mismatch");
}

static void change_file(
		int add, const char *name,
		struct strbuf *diff,
		const char *before, const char *after)
{
	struct strbuf buf = STRBUF_INIT;
	struct cache_entry* ce;
	void *src = NULL;
	unsigned long srcn = 0;
	unsigned char sha1[20];
	char* p;

	if (*name == '/')
		name++;

	p = strrchr(name, '/');
	if (p) {
		/* remove ./.gitempty */
		strbuf_reset(&buf);
		strbuf_add(&buf, name, p - name);
		strbuf_addstr(&buf, "/.gitempty");
		remove_file_from_index(&the_index, buf.buf);
	}

	if (!add) {
		enum object_type type;
		ce = index_name_exists(&svn_index, name, strlen(name), 0);
		if (!ce) goto err;

		src = read_sha1_file(ce->sha1, &type, &srcn);
		if (!src || type != OBJ_BLOB) goto err;
	}

	if (*before)
		checkmd5(before, src, srcn);

	apply_svndiff(&buf, src, srcn, diff->buf, diff->len);

	if (*after)
		checkmd5(after, buf.buf, buf.len);

	if (write_sha1_file(buf.buf, buf.len, "blob", sha1))
		die_errno("write blob");

	ce = make_cache_entry(create_ce_mode(0644), sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&svn_index, ce, ADD_CACHE_OK_TO_ADD);

	if (convert_to_git(name, buf.buf, buf.len, &buf, SAFE_CRLF_FALSE)) {
		if (write_sha1_file(buf.buf, buf.len, "blob", sha1)) {
			die_errno("write blob");
		}
	}

	ce = make_cache_entry(create_ce_mode(0644), sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&the_index, ce, ADD_CACHE_OK_TO_ADD);

	strbuf_release(&buf);
	free(src);
	return;

err:
	die("malformed update");
}

static struct commit *git_checkout;

static void checkout(const char *ref, int rev) {
	struct commit *svn = lookup_commit_reference_by_name(ref);

	while (svn && get_svn_revision(svn) > rev) {
		svn = svn_parent(svn);
	}

	git_checkout = svn_commit(svn);
	svn_checkout_index(&svn_index, svn);
	svn_checkout_index(&the_index, git_checkout);
}

static void reset(void) {
	unsigned char sha1[20];

	git_checkout = NULL;
	svn_checkout_index(&svn_index, NULL);
	svn_checkout_index(&the_index, NULL);

	/* add .gitattributes so the eol behaviour is
	 * maintained. This can be changed on the git side later
	 * if need be. */
	if (svn_eol != EOL_UNSET) {
		struct cache_entry *ce;
		static const char text[] = "* text=auto\n";

		if (write_sha1_file(text, strlen(text), "blob", sha1))
			return;

		ce = make_cache_entry(create_ce_mode(0644), sha1, ".gitattributes", 0, 0);
		if (!ce) return;

		add_index_entry(&the_index, ce, ADD_CACHE_OK_TO_ADD);
	}
}

static void report(const char *ref, const char *gitref) {
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];

	struct commit *svn = lookup_commit_reference_by_name(ref);
	struct commit *git = svn_commit(svn);
	const char *hex;

	if (!prefixcmp(gitref, "refs/tags/")) {
		strbuf_reset(&buf);
		strbuf_addf(&buf, "%s.tag", ref);
		if (read_ref(buf.buf, sha1)) {
			hashcpy(sha1, git->object.sha1);
		}
		hex = sha1_to_hex(sha1);
	} else {
		hex = cmt_to_hex(git);
	}

	if (verbose) {
		fprintf(stderr, "r+ fetched %s %s\n", hex, gitref);
	}

	printf("fetched %s %s\n", hex, gitref);
}

static void havelog(const char *ref, int rev, const char *logrev) {
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];

	struct commit *svn = lookup_commit_reference_by_name(ref);

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s.log", ref);

	if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}

	if (atoi(logrev) == rev)
		return;

	if (get_svn_revision(svn) != rev)
		return;

	if (write_sha1_file(logrev, strlen(logrev), "blob", sha1))
		return;

	update_ref("remote-svn", buf.buf, sha1, null_sha1, 0, QUIET_ON_ERR);
}

static void branch(const char *copyref, int copyrev,
		const char *ref, int rev,
		const char *path, const char *ident, const char *msg)
{
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];
	const char *slash;

	struct commit *svn = lookup_commit_reference_by_name(copyref);
	while (svn && get_svn_revision(svn) > copyrev) {
		svn = svn_parent(svn);
	}

	if (write_svn_commit(NULL, svn_commit(svn), cmt_tree(svn),
				ident, path, rev, sha1)) {
		die_errno("write svn commit");
	}

	update_ref("remote-svn", ref, sha1, null_sha1, 0, DIE_ON_ERR);

	slash = strrchr(path, '/');

	strbuf_reset(&buf);
	strbuf_addf(&buf, "object %s\n"
			"type commit\n"
			"tag %s\n"
			"tagger %s\n"
			"\n"
			"%s",
			cmt_to_hex(svn_commit(svn)),
			slash ? slash+1 : path,
			ident,
			msg);

	strbuf_complete_line(&buf);

	if (!write_sha1_file(buf.buf, buf.len, "tag", sha1)) {
		strbuf_reset(&buf);
		strbuf_addf(&buf, "%s.tag", ref);
		update_ref("remote-svn", buf.buf, sha1, null_sha1, 0, DIE_ON_ERR);
	}

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s.log", ref);
	if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}
}

static void commit(const char *ref, int baserev, int rev,
		const char *path, const char *ident, const char *msg)
{
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];

	struct commit *svn = lookup_commit_reference_by_name(ref);
	struct commit *git = git_checkout;

	if (get_svn_revision(svn) != baserev)
		die("unexpected intermediate commit");

	if (!git || hashcmp(idx_tree(&the_index), cmt_tree(git))) {
		strbuf_reset(&buf);
		strbuf_addf(&buf, "tree %s\n", sha1_to_hex(idx_tree(&the_index)));

		if (git)
			strbuf_addf(&buf, "parent %s\n", cmt_to_hex(git));

		strbuf_addf(&buf,
			"author %s\n"
			"committer %s\n"
			"\n"
			"%s",
			ident, ident, msg);

		strbuf_complete_line(&buf);

		if (write_sha1_file(buf.buf, buf.len, "commit", sha1))
			die_errno("write git commit");

		git = lookup_commit(sha1);
	}

	if (write_svn_commit(svn, git, idx_tree(&svn_index),
				ident, path, rev, sha1)) {
		die_errno("write svn commit");
	}

	update_ref("remote-svn", ref, sha1, svn ? svn->object.sha1 : null_sha1, 0, DIE_ON_ERR);

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s.tag", ref);
	if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}

	strbuf_reset(&buf);
	strbuf_addf(&buf, "%s.log", ref);
	if (!read_ref(buf.buf, sha1)) {
		delete_ref(buf.buf, sha1, 0);
	}
}

static int lookup_cb(const char *refname, const unsigned char *sha1, int flags, void *cb_data) {
	const char *ext = strrchr(refname, '.');
	struct commit *svn;
	int rev = *(int*)cb_data;

	if (!ext || ext[1] < '0' || ext[1] > '9')
		return 0;

	svn = lookup_commit(sha1);
	while (svn && get_svn_revision(svn) > rev) {
		svn = svn_parent(svn);
	}

	if (svn) {
		printf("%s\n", cmt_to_hex(svn_commit(svn)));
		return 1;
	}

	return 0;
}

static void lookup(const char *uuid, const char *path, int rev) {
	static struct strbuf buf = STRBUF_INIT;

	strbuf_reset(&buf);
	strbuf_addf(&buf, "refs/svn/%s", uuid);

	while (*path) {
		int ch = *(path++);
		strbuf_addch(&buf, bad_ref_char(ch) ? '_' : ch);
	}

	if (!for_each_ref_in(buf.buf, &lookup_cb, &rev)) {
		printf("\n");
	}
}

int cmd_remote_svn__helper(int argc, const char **argv, const char *prefix) {
	struct strbuf cmd = STRBUF_INIT;
	struct strbuf ref = STRBUF_INIT;
	struct strbuf gitref = STRBUF_INIT;
	struct strbuf copyref = STRBUF_INIT;
	struct strbuf path = STRBUF_INIT;
	struct strbuf ident = STRBUF_INIT;
	struct strbuf msg = STRBUF_INIT;
	struct strbuf before = STRBUF_INIT;
	struct strbuf after = STRBUF_INIT;
	struct strbuf diff = STRBUF_INIT;
	struct strbuf logrev = STRBUF_INIT;
	struct strbuf uuid = STRBUF_INIT;

	trypause();

	git_config(&config, NULL);
	core_eol = svn_eol;

	for (;;) {
		read_atom(&cmd);

		if (!strcmp(cmd.buf, "")) {
			break;

		} else if (!strcmp(cmd.buf, "verbose")) {
			read_command();
			verbose = 1;

		} else if (!strcmp(cmd.buf, "checkout")) {
			int rev;
			read_string(&ref);
			rev = read_number();
			read_command();

			checkout(ref.buf, rev);

		} else if (!strcmp(cmd.buf, "reset")) {
			read_command();
			reset();

		} else if (!strcmp(cmd.buf, "report")) {
			read_string(&ref);
			read_string(&gitref);
			read_command();

			report(ref.buf, gitref.buf);

		} else if (!strcmp(cmd.buf, "havelog")) {
			int rev;
			read_string(&ref);
			rev = read_number();
			read_atom(&logrev);
			read_command();

			strbuf_complete_line(&logrev);
			havelog(ref.buf, rev, logrev.buf);

		} else if (!strcmp(cmd.buf, "branch")) {
			int copyrev, rev;

			read_string(&copyref);
			copyrev = read_number();
			read_string(&ref);
			rev = read_number();
			read_string(&path);
			read_string(&ident);
			read_string(&msg);
			read_command();

			clean_svn_path(&path);
			branch(copyref.buf, copyrev, ref.buf, rev, path.buf, ident.buf, msg.buf);

		} else if (!strcmp(cmd.buf, "commit")) {
			int baserev, rev;

			read_string(&ref);
			baserev = read_number();
			rev = read_number();
			read_string(&path);
			read_string(&ident);
			read_string(&msg);
			read_command();

			clean_svn_path(&path);
			commit(ref.buf, baserev, rev, path.buf, ident.buf, msg.buf);

		} else if (!strcmp(cmd.buf, "add-dir")) {
			read_string(&path);
			read_command();

			add_dir(path.buf);

		} else if (!strcmp(cmd.buf, "delete-entry")) {
			read_string(&path);
			read_command();

			if (path.len) {
				remove_path_from_index(&svn_index, path.buf+1);
				remove_path_from_index(&the_index, path.buf+1);
			}

		} else if (!strcmp(cmd.buf, "add-file") || !strcmp(cmd.buf, "open-file")) {
			read_string(&path);
			read_string(&before);
			read_string(&after);
			read_string(&diff);
			read_command();

			change_file(cmd.buf[0] == 'a', path.buf, &diff, before.buf, after.buf);

		} else if (!strcmp(cmd.buf, "test")) {
			read_command();
			test_svn_mergeinfo();

		} else if (!strcmp(cmd.buf, "lookup")) {
			int rev;
			strbuf_reset(&path);

			read_atom(&uuid);
			rev = read_number();
			strbuf_getline(&path, stdin, '\n');
			read_command();

			strbuf_trim(&path);
			clean_svn_path(&path);
			lookup(uuid.buf, path.buf, rev);
		}

		fflush(stdout);
	}

	return 0;
}

