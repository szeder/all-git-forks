/*
 * Common refs code for all backends.
 */
#include "cache.h"
#include "refs.h"

int update_ref(const char *action, const char *refname,
	       const unsigned char *sha1, const unsigned char *oldval,
	       int flags, struct strbuf *e)
{
	struct transaction *t;
	struct strbuf err = STRBUF_INIT;

	t = transaction_begin(&err);
	if (!t ||
	    transaction_update_ref(t, refname, sha1, oldval, flags,
				   !!oldval, action, &err) ||
	    transaction_commit(t, &err)) {
		const char *str = "update_ref failed for ref '%s': %s";

		transaction_free(t);
		if (e)
			strbuf_addf(e, str, refname, err.buf);
		strbuf_release(&err);
		return 1;
	}
	strbuf_release(&err);
	transaction_free(t);
	return 0;
}

int delete_ref(const char *refname, const unsigned char *sha1, int delopt)
{
	struct transaction *transaction;
	struct strbuf err = STRBUF_INIT;

	transaction = transaction_begin(&err);
	if (!transaction ||
	    transaction_delete_ref(transaction, refname, sha1, delopt,
				   sha1 && !is_null_sha1(sha1), NULL, &err) ||
	    transaction_commit(transaction, &err)) {
		error("%s", err.buf);
		transaction_free(transaction);
		strbuf_release(&err);
		return 1;
	}
	transaction_free(transaction);
	strbuf_release(&err);
	return 0;
}

int rename_ref(const char *oldrefname, const char *newrefname, const char *logmsg)
{
	unsigned char sha1[20];
	int flag = 0;
	int log;
	struct transaction *transaction = NULL;
	struct strbuf err = STRBUF_INIT;
	struct string_list skip = STRING_LIST_INIT_NODUP;
	const char *symref = NULL;
	struct reflog_committer_info ci;

	memset(&ci, 0, sizeof(ci));
	ci.committer_info = git_committer_info(0);

	symref = resolve_ref_unsafe(oldrefname, RESOLVE_REF_READING,
				    sha1, &flag);
	if (flag & REF_ISSYMREF) {
		error("refname %s is a symbolic ref, renaming it is not "
		      "supported", oldrefname);
		return 1;
	}
	if (!symref) {
		error("refname %s not found", oldrefname);
		return 1;
	}

	string_list_insert(&skip, oldrefname);
	if (!is_refname_available(newrefname, &skip)) {
		string_list_clear(&skip, 0);
		return 1;
	}
	string_list_clear(&skip, 0);

	log = reflog_exists(oldrefname);

	transaction = transaction_begin(&err);
	if (!transaction)
		goto fail;

	if (strcmp(oldrefname, newrefname)) {
		if (transaction_delete_ref(transaction, oldrefname, sha1,
					   REF_NODEREF, 1, NULL, &err))
			goto fail;
		if (log && transaction_rename_reflog(transaction, oldrefname,
						     newrefname, &err))
			goto fail;
		if (log && transaction_update_reflog(transaction, newrefname,
				     sha1, sha1, &ci, logmsg,
				     REFLOG_COMMITTER_INFO_IS_VALID, &err))
			goto fail;
	}

	if (transaction_update_ref(transaction, newrefname, sha1,
				   NULL, 0, 0, NULL, &err))
		goto fail;
	if (transaction_commit(transaction, &err))
		goto fail;
	transaction_free(transaction);
	return 0;

 fail:
	error("rename_ref failed: %s", err.buf);
	strbuf_release(&err);
	transaction_free(transaction);
	return 1;
}
