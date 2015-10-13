/*
 * Reference-related code that is available for use by any reference
 * backend.
 */

#include "cache.h"
#include "refs.h"
#include "refs-be-common.h"

struct ref_transaction *ref_transaction_begin(struct strbuf *err)
{
	assert(err);

	return xcalloc(1, sizeof(struct ref_transaction));
}

void ref_transaction_free(struct ref_transaction *transaction)
{
	int i;

	if (!transaction)
		return;

	for (i = 0; i < transaction->nr; i++) {
		free(transaction->updates[i]->msg);
		free(transaction->updates[i]);
	}
	free(transaction->updates);
	free(transaction);
}

static struct ref_update *add_update(struct ref_transaction *transaction,
				     const char *refname)
{
	size_t len = strlen(refname) + 1;
	struct ref_update *update = xcalloc(1, sizeof(*update) + len);

	memcpy((char *)update->refname, refname, len); /* includes NUL */
	ALLOC_GROW(transaction->updates, transaction->nr + 1, transaction->alloc);
	transaction->updates[transaction->nr++] = update;
	return update;
}

int ref_transaction_update(struct ref_transaction *transaction,
			   const char *refname,
			   const unsigned char *new_sha1,
			   const unsigned char *old_sha1,
			   unsigned int flags, const char *msg,
			   struct strbuf *err)
{
	struct ref_update *update;

	assert(err);

	if (transaction->state != REF_TRANSACTION_OPEN)
		die("BUG: update called for transaction that is not open");

	if (new_sha1 && !is_null_sha1(new_sha1) &&
	    check_refname_format(refname, REFNAME_ALLOW_ONELEVEL)) {
		strbuf_addf(err, "refusing to update ref with bad name %s",
			    refname);
		return -1;
	}

	update = add_update(transaction, refname);
	if (new_sha1) {
		hashcpy(update->new_sha1, new_sha1);
		flags |= REF_HAVE_NEW;
	}
	if (old_sha1) {
		hashcpy(update->old_sha1, old_sha1);
		flags |= REF_HAVE_OLD;
	}
	update->flags = flags;
	if (msg)
		update->msg = xstrdup(msg);
	return 0;
}
