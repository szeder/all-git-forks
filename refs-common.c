/* common code for all ref backends */
#include "cache.h"
#include "refs.h"

int update_ref(const char *action, const char *refname,
	       const unsigned char *sha1, const unsigned char *oldval,
	       int flags, struct strbuf *e)
{
	struct ref_transaction *t;
	struct strbuf err = STRBUF_INIT;

	t = transaction_begin(&err);
	if (!t ||
	    transaction_update_sha1(t, refname, sha1, oldval, flags,
				    !!oldval, action, &err) ||
	    transaction_commit(t, &err)) {
		const char *str = "update_ref failed for ref '%s': %s";

		transaction_free(t);
		if (e)
			strbuf_addf(e, str, refname, err.buf);
		strbuf_release(&err);
		return 1;
	}
	return 0;
}

int delete_ref(const char *refname, const unsigned char *sha1, int delopt)
{
	struct ref_transaction *transaction;
	struct strbuf err = STRBUF_INIT;

	transaction = transaction_begin(&err);
	if (!transaction ||
	    transaction_delete_sha1(transaction, refname, sha1, delopt,
				    sha1 && !is_null_sha1(sha1), NULL, &err) ||
	    transaction_commit(transaction, &err)) {
		error("%s", err.buf);
		transaction_free(transaction);
		strbuf_release(&err);
		return 1;
	}
	transaction_free(transaction);
	return 0;
}

struct rename_reflog_cb {
	struct ref_transaction *transaction;
	const char *refname;
	struct strbuf *err;
};

static int rename_reflog_ent(unsigned char *osha1, unsigned char *nsha1,
			     const char *id, unsigned long timestamp, int tz,
			     const char *message, void *cb_data)
{
	struct rename_reflog_cb *cb = cb_data;
	struct reflog_committer_info ci;

	memset(&ci, 0, sizeof(ci));
	ci.id = id;
	ci.timestamp = timestamp;
	ci.tz = tz;
	return transaction_update_reflog(cb->transaction, cb->refname,
					 nsha1, osha1, &ci, message, 0,
					 cb->err);
}

int rename_ref(const char *oldrefname, const char *newrefname, const char *logmsg)
{
	unsigned char sha1[20];
	int flag = 0, log;
	struct ref_transaction *transaction = NULL;
	struct strbuf err = STRBUF_INIT;
	const char *symref = NULL;
	struct rename_reflog_cb cb;
	struct reflog_committer_info ci;

	memset(&ci, 0, sizeof(ci));
	ci.committer_info = git_committer_info(0);

	symref = resolve_ref_unsafe(oldrefname, sha1,
				    RESOLVE_REF_READING, &flag);
	if (flag & REF_ISSYMREF) {
		error("refname %s is a symbolic ref, renaming it is not supported",
			oldrefname);
		return 1;
	}
	if (!symref) {
		error("refname %s not found", oldrefname);
		return 1;
	}

	if (!is_refname_available(newrefname, &oldrefname, 1))
		return 1;

	log = reflog_exists(oldrefname);
	transaction = transaction_begin(&err);
	if (!transaction)
		goto fail;

	if (strcmp(oldrefname, newrefname)) {
		if (log && transaction_update_reflog(transaction, newrefname,
						     sha1, sha1, &ci, NULL,
						     REFLOG_TRUNCATE, &err))
			goto fail;
		cb.transaction = transaction;
		cb.refname = newrefname;
		cb.err = &err;
		if (log && for_each_reflog_ent(oldrefname, rename_reflog_ent,
					       &cb))
			goto fail;

		if (transaction_delete_sha1(transaction, oldrefname, sha1,
					    REF_NODEREF,
					    1, NULL, &err))
			goto fail;
	}
	if (transaction_update_sha1(transaction, newrefname, sha1,
				    NULL, 0, 0, NULL, &err))
		goto fail;
	if (log && transaction_update_reflog(transaction, newrefname, sha1,
					     sha1, &ci, logmsg,
					     REFLOG_COMMITTER_INFO_IS_VALID,
					     &err))
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
