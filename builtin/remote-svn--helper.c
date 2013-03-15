#include "cache.h"
#include "builtin.h"
#include "svn.h"
#include "quote.h"
#include "refs.h"
#include "cache-tree.h"
#include <openssl/md5.h>

static struct index_state svn_index;
static int svn_eol = EOL_UNSET;

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

static char* next_arg(char *arg, char **endp) {
	char *p;
	arg += strspn(arg, " ");
	p = arg + strcspn(arg, " \n");
	if (*p) *(p++) = '\0';
	*endp = p;
	return arg;
}

static char* unquote_arg(char *arg, char **endp) {
	static struct strbuf buf = STRBUF_INIT;

	arg += strspn(arg, " ");

	strbuf_reset(&buf);
	if (unquote_c_style(&buf, arg, (const char**) endp)) {
		return next_arg(arg, endp);
	}

	if (*endp - arg <= buf.len)
		die("unquoting didn't contract");
	memcpy(arg, buf.buf, buf.len+1);
	return arg;
}

static void add_dir(const char *name) {
	struct strbuf buf = STRBUF_INIT;
	struct cache_entry* ce;
	char *p = strrchr(name, '/');
	unsigned char sha1[20];

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
	char* p = strrchr(name, '/');
	void *src = NULL;
	unsigned long srcn = 0;
	unsigned char sha1[20];

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

	ce = make_cache_entry(0644, sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&svn_index, ce, ADD_CACHE_OK_TO_ADD);

	if (convert_to_git(name, buf.buf, buf.len, &buf, SAFE_CRLF_FALSE)) {
		if (write_sha1_file(buf.buf, buf.len, "blob", sha1)) {
			die_errno("write blob");
		}
	}

	ce = make_cache_entry(0644, sha1, name, 0, 0);
	if (!ce) die("make_cache_entry failed for path '%s'", name);
	add_index_entry(&the_index, ce, ADD_CACHE_OK_TO_ADD);

	strbuf_release(&buf);
	free(src);
	return;

err:
	die("malformed update");
}

static void delete_entry(const char *name) {
	remove_path_from_index(&svn_index, name);
	remove_path_from_index(&the_index, name);
}

static struct commit *checkout_git;
static struct mergeinfo *mergeinfo, *svn_mergeinfo;

static void command(char *cmd, char *arg) {
	static struct strbuf buf = STRBUF_INIT;
	unsigned char sha1[20];

	if (!strcmp(cmd, "checkout")) {
		char *ref = next_arg(arg, &arg);
		int rev = strtol(arg, &arg, 10);
		struct commit *svn = lookup_commit_reference_by_name(ref);

		while (svn && get_svn_revision(svn) > rev) {
			svn = svn_parent(svn);
		}

		checkout_git = svn_commit(svn);
		mergeinfo = get_mergeinfo(svn);
		svn_mergeinfo = get_svn_mergeinfo(svn);

		svn_checkout_index(&svn_index, svn);
		svn_checkout_index(&the_index, checkout_git);

	} else if (!strcmp(cmd, "reset")) {
		svn_checkout_index(&svn_index, NULL);
		svn_checkout_index(&the_index, NULL);

		checkout_git = NULL;
		mergeinfo = parse_svn_mergeinfo("");
		svn_mergeinfo = parse_svn_mergeinfo("");

		/* add .gitattributes so the eol behaviour is
		 * maintained. This can be changed on the git side later
		 * if need be. */
		if (svn_eol != EOL_UNSET) {
			struct cache_entry *ce;
			static const char text[] = "* text=auto\n";

			if (write_sha1_file(text, strlen(text), "blob", sha1))
				return;

			ce = make_cache_entry(0644, sha1, ".gitattributes", 0, 0);
			if (!ce) return;

			add_index_entry(&the_index, ce, ADD_CACHE_OK_TO_ADD);
		}

	} else if (!strcmp(cmd, "report")) {
		char *ref = next_arg(arg, &arg);
		struct commit *svn = lookup_commit_reference_by_name(ref);
		struct commit *git = svn_commit(svn);

		strbuf_reset(&buf);
		strbuf_addf(&buf, "%s.tag", ref);
		if (read_ref(buf.buf, sha1)) {
			hashcpy(sha1, git->object.sha1);
		}

		for (;;) {
			char *gref = next_arg(arg, &arg);
			if (!gref || !*gref) break;

			if (!prefixcmp(gref, "refs/tags/")) {
				printf("fetched %s %s\n", sha1_to_hex(sha1), gref);
			} else {
				printf("fetched %s %s\n", cmt_to_hex(git), gref);
			}
		}

	} else if (!strcmp(cmd, "branch")) {
		char *fref = next_arg(arg, &arg);
		int frev = strtol(arg, &arg, 10);
		char *tref = next_arg(arg, &arg);
		int trev = strtol(arg, &arg, 10);
		char *path = unquote_arg(arg, &arg);
		char *ident = unquote_arg(arg, &arg);
		int msglen = strtol(arg, &arg, 10);

		char *slash = strrchr(path, '/');
		struct commit *svn = lookup_commit_reference_by_name(fref);

		while (svn && get_svn_revision(svn) > frev) {
			svn = svn_parent(svn);
		}

		mergeinfo = get_mergeinfo(svn);
		svn_mergeinfo = get_svn_mergeinfo(svn);
		add_svn_mergeinfo(mergeinfo, path, trev, trev);

		if (write_svn_commit(NULL, svn_commit(svn), cmt_tree(svn), ident, path, trev, mergeinfo, svn_mergeinfo, sha1))
			die_errno("write svn commit");

		free_svn_mergeinfo(mergeinfo);
		free_svn_mergeinfo(svn_mergeinfo);
		mergeinfo = svn_mergeinfo = NULL;

		update_ref("remote-svn", tref, sha1, null_sha1, 0, DIE_ON_ERR);

		strbuf_reset(&buf);
		strbuf_addf(&buf,
			"object %s\n"
			"type commit\n"
			"tag %s\n"
			"tagger %s\n"
			"\n",
			cmt_to_hex(svn_commit(svn)),
			slash ? slash+1 : path,
			ident);

		if (strbuf_fread(&buf, msglen, stdin) != msglen)
			die_errno("read");

		strbuf_complete_line(&buf);

		if (!write_sha1_file(buf.buf, buf.len, "tag", sha1)) {
			strbuf_reset(&buf);
			strbuf_addf(&buf, "%s.tag", tref);
			update_ref("remote-svn", buf.buf, sha1, null_sha1, 0, DIE_ON_ERR);
		}

	} else if (!strcmp(cmd, "commit")) {
		char *ref = next_arg(arg, &arg);
		int baserev = strtol(arg, &arg, 10);
		int newrev = strtol(arg, &arg, 10);
		char *path = unquote_arg(arg, &arg);
		char *ident = unquote_arg(arg, &arg);
		int msglen = strtol(arg, &arg, 10);
		struct commit *svn = lookup_commit_reference_by_name(ref);
		struct commit *git = checkout_git;

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
				"\n",
				ident, ident);

			if (strbuf_fread(&buf, msglen, stdin) != msglen)
				die_errno("read");

			strbuf_complete_line(&buf);

			if (write_sha1_file(buf.buf, buf.len, "commit", sha1))
				die_errno("write git commit");

			git = lookup_commit(sha1);
		}

		add_svn_mergeinfo(mergeinfo, path, baserev+1, newrev);

		if (write_svn_commit(svn, git, idx_tree(&svn_index), ident, path, newrev, mergeinfo, svn_mergeinfo, sha1))
			die_errno("write svn commit");

		update_ref("remote-svn", ref, sha1, svn ? svn->object.sha1 : null_sha1, 0, DIE_ON_ERR);

		strbuf_reset(&buf);
		strbuf_addf(&buf, "%s.tag", ref);
		if (!read_ref(buf.buf, sha1)) {
			delete_ref(buf.buf, sha1, 0);
		}

		free_svn_mergeinfo(mergeinfo);
		free_svn_mergeinfo(svn_mergeinfo);
		mergeinfo = svn_mergeinfo = NULL;

	} else if (!strcmp(cmd, "add-dir")) {
		char *path = unquote_arg(arg, &arg);
		if (*path == '/') path++;
		add_dir(path);

	} else if (!strcmp(cmd, "delete-entry")) {
		char *path = unquote_arg(arg, &arg);
		if (*path == '/') path++;
		delete_entry(path);

	} else if (!strcmp(cmd, "add-file") || !strcmp(cmd, "open-file")) {
		char *path = unquote_arg(arg, &arg);
		int dlen = strtol(arg, &arg, 10);
		char *before = unquote_arg(arg, &arg);
		char *after = unquote_arg(arg, &arg);

		strbuf_reset(&buf);
		if (strbuf_fread(&buf, dlen, stdin) != dlen)
			die_errno("read");

		if (*path == '/') path++;
		change_file(cmd[0] == 'a', path, &buf, before, after);

	} else if (!strcmp(cmd, "set-mergeinfo")) {
		char *info = unquote_arg(arg, &arg);
		free_svn_mergeinfo(svn_mergeinfo);
		svn_mergeinfo = parse_svn_mergeinfo(info);
	}
}

int cmd_remote_svn__update(int argc, const char **argv, const char *prefix) {
	struct commit *svn = lookup_commit_reference_by_name(*(++argv));
	const char *path = get_svn_path(svn);
	struct commit_list *cmts = NULL;
	struct strbuf author = STRBUF_INIT;
	struct strbuf ref = STRBUF_INIT;
	int start = 0;

	/* old setup is svn first, git second */
	for (;;) {
		struct commit *git;
		int rev = get_svn_revision(svn);
		if (!rev) break;
		start = rev;
		if (svn->parents->next) {
			git = svn->parents->next->item;
			git->util = svn;
			commit_list_insert(git, &cmts);
		} else {
			cmts = NULL;
		}
		svn = svn->parents->item;
	}

	svn = NULL;

	/* delete single commit refs, so the tag gets recreated */
	if (!cmts->next && cmts->item->parents)
		return 0;

	strbuf_addstr(&ref, *argv);
	strbuf_setlen(&ref, ref.len - 4);
	strbuf_addf(&ref, ".%d", start);

	while (cmts) {
		unsigned char sha1[20];
		struct commit *git = cmts->item;
		struct commit *oldsvn = git->util;
		char *p = strstr(oldsvn->buffer, "\nauthor ") + strlen("\nauthor ");

		strbuf_reset(&author);
		strbuf_add(&author, p, strcspn(p, "\n"));

		if (write_svn_commit(svn, git, cmt_tree(oldsvn), author.buf, path, get_svn_revision(oldsvn), NULL, NULL, sha1))
			die_errno("write svn commit");

		update_ref("remote-svn", ref.buf, sha1, svn ? svn->object.sha1 : null_sha1, 0, DIE_ON_ERR);

		svn = lookup_commit(sha1);
		cmts = cmts->next;
	}

	return 0;
}

int cmd_remote_svn__helper(int argc, const char **argv, const char *prefix) {
	struct strbuf buf = STRBUF_INIT;

	trypause();

	git_config(&config, NULL);
	core_eol = svn_eol;

	while (strbuf_getline(&buf, stdin, '\n') != EOF) {
		char *cmd = buf.buf;
		char *arg = buf.buf + strcspn(cmd, " \n");
		if (*arg) *(arg++) = '\0';
		command(cmd, arg);
		fflush(stdout);
	}

	return 0;
}

