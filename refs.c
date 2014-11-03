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

struct read_ref_at_cb {
	const char *refname;
	unsigned long at_time;
	int cnt;
	int reccnt;
	unsigned char *sha1;
	int found_it;

	unsigned char osha1[20];
	unsigned char nsha1[20];
	int tz;
	unsigned long date;
	char **msg;
	unsigned long *cutoff_time;
	int *cutoff_tz;
	int *cutoff_cnt;
};

static int read_ref_at_ent(unsigned char *osha1, unsigned char *nsha1,
		const char *id, unsigned long timestamp, int tz,
		const char *message, void *cb_data)
{
	struct read_ref_at_cb *cb = cb_data;

	cb->reccnt++;
	cb->tz = tz;
	cb->date = timestamp;

	if (timestamp <= cb->at_time || cb->cnt == 0) {
		if (cb->msg)
			*cb->msg = xstrdup(message);
		if (cb->cutoff_time)
			*cb->cutoff_time = timestamp;
		if (cb->cutoff_tz)
			*cb->cutoff_tz = tz;
		if (cb->cutoff_cnt)
			*cb->cutoff_cnt = cb->reccnt - 1;
		/*
		 * we have not yet updated cb->[n|o]sha1 so they still
		 * hold the values for the previous record.
		 */
		if (!is_null_sha1(cb->osha1)) {
			hashcpy(cb->sha1, nsha1);
			if (hashcmp(cb->osha1, nsha1))
				warning("Log for ref %s has gap after %s.",
					cb->refname, show_date(cb->date, cb->tz, DATE_RFC2822));
		}
		else if (cb->date == cb->at_time)
			hashcpy(cb->sha1, nsha1);
		else if (hashcmp(nsha1, cb->sha1))
			warning("Log for ref %s unexpectedly ended on %s.",
				cb->refname, show_date(cb->date, cb->tz,
						   DATE_RFC2822));
		hashcpy(cb->osha1, osha1);
		hashcpy(cb->nsha1, nsha1);
		cb->found_it = 1;
		return 1;
	}
	hashcpy(cb->osha1, osha1);
	hashcpy(cb->nsha1, nsha1);
	if (cb->cnt > 0)
		cb->cnt--;
	return 0;
}

static int read_ref_at_ent_oldest(unsigned char *osha1, unsigned char *nsha1,
				  const char *id, unsigned long timestamp,
				  int tz, const char *message, void *cb_data)
{
	struct read_ref_at_cb *cb = cb_data;

	if (cb->msg)
		*cb->msg = xstrdup(message);
	if (cb->cutoff_time)
		*cb->cutoff_time = timestamp;
	if (cb->cutoff_tz)
		*cb->cutoff_tz = tz;
	if (cb->cutoff_cnt)
		*cb->cutoff_cnt = cb->reccnt;
	hashcpy(cb->sha1, osha1);
	if (is_null_sha1(cb->sha1))
		hashcpy(cb->sha1, nsha1);
	/* We just want the first entry */
	return 1;
}

int read_ref_at(const char *refname, unsigned int flags, unsigned long at_time, int cnt,
		unsigned char *sha1, char **msg,
		unsigned long *cutoff_time, int *cutoff_tz, int *cutoff_cnt)
{
	struct read_ref_at_cb cb;

	memset(&cb, 0, sizeof(cb));
	cb.refname = refname;
	cb.at_time = at_time;
	cb.cnt = cnt;
	cb.msg = msg;
	cb.cutoff_time = cutoff_time;
	cb.cutoff_tz = cutoff_tz;
	cb.cutoff_cnt = cutoff_cnt;
	cb.sha1 = sha1;

	for_each_reflog_ent_reverse(refname, read_ref_at_ent, &cb);

	if (!cb.reccnt) {
		if (flags & GET_SHA1_QUIETLY)
			exit(128);
		else
			die("Log for %s is empty.", refname);
	}
	if (cb.found_it)
		return 0;

	for_each_reflog_ent(refname, read_ref_at_ent_oldest, &cb);

	return 1;
}

static struct string_list *hide_refs;

int parse_hide_refs_config(const char *var, const char *value, const char *section)
{
	if (!strcmp("transfer.hiderefs", var) ||
	    /* NEEDSWORK: use parse_config_key() once both are merged */
	    (starts_with(var, section) && var[strlen(section)] == '.' &&
	     !strcmp(var + strlen(section), ".hiderefs"))) {
		char *ref;
		int len;

		if (!value)
			return config_error_nonbool(var);
		ref = xstrdup(value);
		len = strlen(ref);
		while (len && ref[len - 1] == '/')
			ref[--len] = '\0';
		if (!hide_refs) {
			hide_refs = xcalloc(1, sizeof(*hide_refs));
			hide_refs->strdup_strings = 1;
		}
		string_list_append(hide_refs, ref);
	}
	return 0;
}

int ref_is_hidden(const char *refname)
{
	struct string_list_item *item;

	if (!hide_refs)
		return 0;
	for_each_string_list_item(item, hide_refs) {
		int len;
		if (!starts_with(refname, item->string))
			continue;
		len = strlen(item->string);
		if (!refname[len] || refname[len] == '/')
			return 1;
	}
	return 0;
}
