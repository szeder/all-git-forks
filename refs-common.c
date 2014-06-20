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

