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
