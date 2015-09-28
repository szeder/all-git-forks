/*
 * Common refs code for all backends.
 */
#include "cache.h"
#include "refs.h"
#include "lockfile.h"

static int is_per_worktree_ref(const char *refname)
{
	return !strcmp(refname, "HEAD") ||
		starts_with(refname, "refs/bisect/");
}

static int is_pseudoref_syntax(const char *refname)
{
	const char *c;

	for (c = refname; *c; c++) {
		if (!isupper(*c) && *c != '-' && *c != '_')
			return 0;
	}

	return 1;
}

enum ref_type ref_type(const char *refname)
{
	if (is_per_worktree_ref(refname))
		return REF_TYPE_PER_WORKTREE;
	if (is_pseudoref_syntax(refname))
		return REF_TYPE_PSEUDOREF;
       return REF_TYPE_NORMAL;
}

static int write_pseudoref(const char *pseudoref, const unsigned char *sha1,
			   const unsigned char *old_sha1, struct strbuf *err)
{
	const char *filename;
	int fd;
	static struct lock_file lock;
	struct strbuf buf = STRBUF_INIT;
	int ret = -1;

	strbuf_addf(&buf, "%s\n", sha1_to_hex(sha1));

	filename = git_path("%s", pseudoref);
	fd = hold_lock_file_for_update(&lock, filename, LOCK_DIE_ON_ERROR);
	if (fd < 0) {
		strbuf_addf(err, "Could not open '%s' for writing: %s",
			    filename, strerror(errno));
		return -1;
	}

	if (old_sha1) {
		unsigned char actual_old_sha1[20];
		read_ref(pseudoref, actual_old_sha1);
		if (hashcmp(actual_old_sha1, old_sha1)) {
			strbuf_addf(err, "Unexpected sha1 when writing %s", pseudoref);
			rollback_lock_file(&lock);
			goto done;
		}
	}

	if (write_in_full(fd, buf.buf, buf.len) != buf.len) {
		strbuf_addf(err, "Could not write to '%s'", filename);
		rollback_lock_file(&lock);
		goto done;
	}

	commit_lock_file(&lock);
	ret = 0;
done:
	strbuf_release(&buf);
	return ret;
}

int update_ref(const char *msg, const char *refname,
	       const unsigned char *new_sha1, const unsigned char *old_sha1,
	       unsigned int flags, enum action_on_err onerr)
{
	struct ref_transaction *t = NULL;
	struct strbuf err = STRBUF_INIT;
	int ret = 0;

	if (ref_type(refname) == REF_TYPE_PSEUDOREF) {
		ret = write_pseudoref(refname, new_sha1, old_sha1, &err);
	} else {
		t = ref_transaction_begin(&err);
		if (!t ||
		    ref_transaction_update(t, refname, new_sha1, old_sha1,
					   flags, msg, &err) ||
		    ref_transaction_commit(t, &err)) {
			ret = 1;
			ref_transaction_free(t);
		}
	}

	if (ret) {
		const char *str = "update_ref failed for ref '%s': %s";

		switch (onerr) {
		case UPDATE_REFS_MSG_ON_ERR:
			error(str, refname, err.buf);
			break;
		case UPDATE_REFS_DIE_ON_ERR:
			die(str, refname, err.buf);
			break;
		case UPDATE_REFS_QUIET_ON_ERR:
			break;
		}
		strbuf_release(&err);
		return 1;
	}
	strbuf_release(&err);
	if (t)
		ref_transaction_free(t);
	return 0;
}


static int delete_pseudoref(const char *pseudoref, const unsigned char *old_sha1)
{
	static struct lock_file lock;
	const char *filename;

	filename = git_path("%s", pseudoref);

	if (old_sha1 && !is_null_sha1(old_sha1)) {
		int fd;
		unsigned char actual_old_sha1[20];

		fd = hold_lock_file_for_update(&lock, filename,
					       LOCK_DIE_ON_ERROR);
		if (fd < 0)
			die_errno(_("Could not open '%s' for writing"), filename);
		read_ref(pseudoref, actual_old_sha1);
		if (hashcmp(actual_old_sha1, old_sha1)) {
			warning("Unexpected sha1 when deleting %s", pseudoref);
			rollback_lock_file(&lock);
			return -1;
		}

		unlink(filename);
		rollback_lock_file(&lock);
	} else {
		unlink(filename);
	}

	return 0;
}

int delete_ref(const char *refname, const unsigned char *old_sha1,
	       unsigned int flags)
{
	struct ref_transaction *transaction;
	struct strbuf err = STRBUF_INIT;

	if (ref_type(refname) == REF_TYPE_PSEUDOREF)
		return delete_pseudoref(refname, old_sha1);

	transaction = ref_transaction_begin(&err);
	if (!transaction ||
	    ref_transaction_delete(transaction, refname, old_sha1,
				   flags, NULL, &err) ||
	    ref_transaction_commit(transaction, &err)) {
		error("%s", err.buf);
		ref_transaction_free(transaction);
		strbuf_release(&err);
		return 1;
	}
	ref_transaction_free(transaction);
	strbuf_release(&err);
	return 0;
}

int delete_refs(struct string_list *refnames)
{
	struct strbuf err = STRBUF_INIT;
	int i, result = 0;

	if (!refnames->nr)
		return 0;

	result = repack_without_refs(refnames, &err);
	if (result) {
		/*
		 * If we failed to rewrite the packed-refs file, then
		 * it is unsafe to try to remove loose refs, because
		 * doing so might expose an obsolete packed value for
		 * a reference that might even point at an object that
		 * has been garbage collected.
		 */
		if (refnames->nr == 1)
			error(_("could not delete reference %s: %s"),
			      refnames->items[0].string, err.buf);
		else
			error(_("could not delete references: %s"), err.buf);

		goto out;
	}

	for (i = 0; i < refnames->nr; i++) {
		const char *refname = refnames->items[i].string;

		if (delete_ref(refname, NULL, 0))
			result |= error(_("could not remove reference %s"), refname);
	}

out:
	strbuf_release(&err);
	return result;
}
