/*
 * This file implements a DataBase backend for refs.
 *
 * Any ref operations are marshalled and passed to a separate daemon
 * across a unix domain socket. As part of the protocol we also introduce
 * a "repository name" configuration option. This allows multiple
 * git repositories to share one single always running database daemon
 * and allow storing the data for multiple repositories in a single location.
 */
#include <sys/uio.h>
#include "cache.h"
#include "object.h"
#include "refs.h"
#include "tag.h"

static int dbs = -1;
static int inside_transaction;

/*
 * Transaction states.
 * OPEN:   The transaction is in a valid state and can accept new updates.
 *         An OPEN transaction can be committed.
 * CLOSED: If an open transaction is successfully committed the state will
 *         change to CLOSED. No further changes can be made to a CLOSED
 *         transaction.
 *         CLOSED means that all updates have been successfully committed and
 *         the only thing that remains is to free the completed transaction.
 * ERROR:  The transaction has failed and is no longer committable.
 *         No further changes can be made to a CLOSED transaction and it must
 *         be rolled back using transaction_free.
 */
enum transaction_state {
	TRANSACTION_OPEN   = 0,
	TRANSACTION_CLOSED = 1,
	TRANSACTION_ERROR  = 2,
};

/*
 * Data structure for holding a reference transaction.
 * This structure is opaque to callers.
 */
struct transaction {
	enum transaction_state state;
};

#define REFS_DB "refs"
#define LOGS_DB "logs"

static int execute_command(struct strbuf *cmd, struct strbuf *outdata,
			   struct strbuf *err)
{
	struct strbuf data = STRBUF_INIT;
	int pos = 0, count, is_error;
	uint32_t len;
	struct iovec iov[2];

	len = htonl(cmd->len);
	while (pos < cmd->len + 4) {
		if (pos < 4) {
			iov[0].iov_base = ((char *)&len) + pos;
			iov[0].iov_len = 4 - pos;
			iov[1].iov_base = cmd->buf;
			iov[1].iov_len = cmd->len;
		} else {
			iov[0].iov_base = cmd->buf + pos - 4;
			iov[0].iov_len = cmd->len - pos + 4;
		}
		count = writev(dbs, iov, pos < 4 ? 2 : 1);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == -1)
			die("lost connection to database: %s", strerror(errno));
		pos += count;
	}

	/* read the reply */
	pos = 0;
	while (pos < 4) {
		count = read(dbs, &len, 4 - pos);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == 0 || count == -1)
			die("lost connection to database: %s", strerror(errno));
		pos += count;
	}
	len = ntohl(len);
	is_error = !!(len & 0x80000000);
	len &= 0x7fffffff;

	strbuf_grow(&data, len);
	strbuf_setlen(&data, len);

	pos = 0;
	while (pos < len) {
		count = read(dbs, data.buf + pos, len - pos);
		if (count == -1 && errno == EINTR)
			continue;
		if (count == 0 || count == -1)
			die("lost connection to database: %s", strerror(errno));
		pos += count;
	}

	if (is_error) {
		if (err)
			*err = data;
		else
			strbuf_release(&data);
		return -1;
	} else if (outdata)
		*outdata = data;
	else
		strbuf_release(&data);

	return 0;
}

static int DB_store(const char *db, struct strbuf *key, struct strbuf *data,
		    struct strbuf *err)
{
	struct strbuf cmd = STRBUF_INIT;
	uint32_t len;
	int ret;

	strbuf_addf(&cmd, "store%c%s%c", '\0', db, '\0');
	len = htonl(key->len);
	strbuf_add(&cmd, &len, sizeof(uint32_t));
	strbuf_addbuf(&cmd, key);
	strbuf_addbuf(&cmd, data);
	ret = execute_command(&cmd, NULL, err);

	strbuf_release(&cmd);
	return ret;
}

static int DB_transaction_start(struct strbuf *err)
{
	int ret;
	struct strbuf cmd = STRBUF_INIT;

	strbuf_addf(&cmd, "start%c", '\0');
	ret = execute_command(&cmd, NULL, err);

	strbuf_release(&cmd);
	return ret;
}

static void DB_transaction_cancel(void)
{
	struct strbuf cmd = STRBUF_INIT;

	strbuf_addf(&cmd, "cancel%c", '\0');
	execute_command(&cmd, NULL, NULL);

	strbuf_release(&cmd);
}

static int DB_transaction_commit(struct transaction *transaction,
				  struct strbuf *err)
{
	int ret;
	struct strbuf cmd = STRBUF_INIT;

	strbuf_addf(&cmd, "commit%c", '\0');
	ret = execute_command(&cmd, NULL, err);

	strbuf_release(&cmd);
	return ret;
}

static int DB_delete(const char *db, struct strbuf *key, struct strbuf *err)
{
	int ret;
	struct strbuf cmd = STRBUF_INIT;

	strbuf_addf(&cmd, "delete%c%s%c", '\0', db, '\0');
	strbuf_addbuf(&cmd, key);
	ret = execute_command(&cmd, NULL, err);

	strbuf_release(&cmd);
	return ret;
}

static struct strbuf DB_fetch(const char *db, struct strbuf *key)
{
	struct strbuf cmd = STRBUF_INIT;
	struct strbuf err = STRBUF_INIT;
	struct strbuf data = STRBUF_INIT;

	strbuf_addf(&cmd, "fetch%c%s%c", '\0', db, '\0');
	strbuf_addbuf(&cmd, key);
	if (execute_command(&cmd, &data, &err)) {
		error("%s", err.buf);
		strbuf_release(&err);
	}
	strbuf_release(&cmd);

	/* The caller will release data for us */
	return data;
}

typedef int (*db_traverse_func)(struct strbuf *key, struct strbuf *data,
				void *private_data);

static int DB_traverse_read(const char *dbprefix,
			    const char *db, db_traverse_func fn,
			    const char *base,
			    void *private_data,
			    struct strbuf *err)
{
	struct strbuf cmd = STRBUF_INIT;
	struct strbuf data = STRBUF_INIT;
	struct strbuf k;
	struct strbuf d;
	int ret = 0, pos;

	strbuf_addf(&cmd, "traverse%c%s%c%s%c%s%c", '\0', db, '\0',
		    dbprefix, '\0', base, '\0');
	if (execute_command(&cmd, &data, err)) {
		strbuf_release(&cmd);
		return -1;
	}

	pos = 0;
	while (!ret && pos < data.len) {
		uint32_t len;

		if (pos > data.len - 4) {
			ret = -1;
			goto finished;
		}

		memcpy(&len, &data.buf[pos], sizeof(len));
		pos += 4;
		len = ntohl(len);
		if (len + pos > data.len) {
			ret = -1;
			goto finished;
		}

		k.len = len;
		k.buf = data.buf + pos;
		pos += len;

		if (pos > data.len - 4) {
			ret = -1;
			goto finished;
		}

		memcpy(&len, &data.buf[pos], sizeof(len));
		pos += 4;
		len = ntohl(len);
		if (len + pos > data.len) {
			ret = -1;
			goto finished;
		}

		d.len = len;
		d.buf = data.buf + pos;
		pos += len;

		ret = fn(&k, &d, private_data);
	}

 finished:
	strbuf_release(&cmd);
	strbuf_release(&data);
	return ret;
}

static int delete_refname(const char *db, const char *refname,
			  struct strbuf *err)
{
	struct strbuf key = STRBUF_INIT;
	int ret;

	strbuf_addf(&key, "%s%c%s%c", db_repo_name, '\0', refname, '\0');

	ret = DB_delete(db, &key, err);
	strbuf_release(&key);
	return ret;
}

static struct strbuf fetch_refname(const char *dbprefix, const char *db,
				   const char *refname)
{
	struct strbuf key = STRBUF_INIT;
	struct strbuf data = STRBUF_INIT;

	strbuf_addf(&key, "%s%c%s%c", dbprefix, '\0', refname, '\0');

	data = DB_fetch(db, &key);
	strbuf_release(&key);
	return data;
}

static int store_refname(const char *db, const char *refname,
			 struct strbuf *data, struct strbuf *err)
{
	int ret;
	struct strbuf key = STRBUF_INIT;

	strbuf_addf(&key, "%s%c%s%c", db_repo_name, '\0', refname, '\0');

	ret = DB_store(db, &key, data, err);
	strbuf_release(&key);
	return ret;
}

static int gitdb_pack_refs(unsigned int flags, struct strbuf *err)
{
	strbuf_addf(err, "pack_refs is not available on the DB backend.");
	return -1;
}

static uint64_t ntoh64(uint64_t val)
{
	uint64_t tmp = val;
	unsigned char *c = (unsigned char *)&tmp;

	val = c[0];
	val = (val << 8) | c[1];
	val = (val << 8) | c[2];
	val = (val << 8) | c[3];
	val = (val << 8) | c[4];
	val = (val << 8) | c[5];
	val = (val << 8) | c[6];
	val = (val << 8) | c[7];
	return val;
}

static uint64_t hton64(uint64_t val)
{
	uint64_t tmp = 0;
	unsigned char *c = (unsigned char *)&tmp;

	c[0] = val >> 56;
	c[1] = val >> 48;
	c[2] = val >> 40;
	c[3] = val >> 32;
	c[4] = val >> 24;
	c[5] = val >> 16;
	c[6] = val >>  8;
	c[7] = val;
	return tmp;
}

static int store_seqnum(const char *db, uint64_t seqnum, struct strbuf *err)
{
	struct strbuf key, data;
	uint64_t val = hton64(seqnum);

	key.buf = (char *)"\0seqnum";
	key.len = strlen("seqnum") + 1;

	data.buf = (char *)&val;
	data.len = sizeof(val);

	return DB_store(db, &key, &data, err);
}

static uint64_t fetch_seqnum(const char *db)
{
	struct strbuf key, data;
	uint64_t val = 1;

	key.buf = (char *)"\0seqnum";
	key.len = strlen("seqnum") + 1;

	data = DB_fetch(db, &key);
	if (data.len == sizeof(val))
		val = ntoh64(*(uint64_t *)data.buf);
	strbuf_release(&data);
	return val;
}


static int log_ref_write(const char *refname, const unsigned char *old_sha1,
			 const unsigned char *new_sha1,
			 const char *committer, const char *msg,
			 struct strbuf *err)
{
	uint64_t seqnum = fetch_seqnum(LOGS_DB), tmp;
	struct strbuf data = STRBUF_INIT;
	struct strbuf logrec = STRBUF_INIT;
	struct strbuf key;
	int result = 0;

	if (!msg)
		return 0;

	if (log_all_ref_updates < 0)
		log_all_ref_updates = !is_bare_repository();

	if (log_all_ref_updates && !reflog_exists(refname))
	  result = create_reflog(refname, err);

	if (result)
		return result;

	if (!reflog_exists(refname))
		return 0;

	if (store_seqnum(LOGS_DB, seqnum + 1, err))
		goto failed;

	strbuf_add(&logrec, old_sha1 ? old_sha1 : null_sha1, 20);
	strbuf_add(&logrec, new_sha1, 20);
	strbuf_addf(&logrec, "%s%c", committer, '\0');
	if (msg)
		strbuf_addf(&logrec, "%s", msg);

	if (logrec.buf[logrec.len - 1] != '\n')
		strbuf_addf(&logrec, "%c%c", '\n', '\0');
	else
		strbuf_addf(&logrec, "%c", '\0');

	tmp = hton64(seqnum);
	key.buf = (char *)&tmp;
	key.len = sizeof(tmp);

	if (DB_store(LOGS_DB, &key, &logrec, err))
		goto failed;

	data = fetch_refname(db_repo_name, LOGS_DB, refname);
	if (data.len % sizeof(uint64_t)) {
		strbuf_release(&data);
		strbuf_addf(err, "log record is not a multiple of 8: %s",
			    refname);
		goto failed;
	}
	strbuf_add(&data, &tmp, sizeof(uint64_t));

	if (store_refname(LOGS_DB, refname, &data, err))
		goto failed;

	strbuf_release(&logrec);
	strbuf_release(&data);
	return 0;

failed:
	strbuf_release(&logrec);
	strbuf_release(&data);
	return -1;
}

static int gitdb_create_symref(const char *ref_target,
			       const char *refs_heads_master,
			       const char *logmsg,
			       struct strbuf *err)
{
	char ref[1000];
	int len;
	unsigned char old_sha1[20], new_sha1[20];
	struct strbuf data;

	if (logmsg && read_ref(ref_target, old_sha1))
		hashclr(old_sha1);

	len = snprintf(ref, sizeof(ref), "ref: %s", refs_heads_master);
	if (sizeof(ref) <= len) {
		error("refname too long: %s", refs_heads_master);
		return -1;
	}

	data.buf = (char *)ref;
	data.len = strlen(ref) + 1;
	if (store_refname(REFS_DB, ref_target, &data, err))
		return -1;

	if (logmsg && !read_ref(refs_heads_master, new_sha1) &&
	    log_ref_write(ref_target, old_sha1, new_sha1,
			  git_committer_info(0), logmsg, err))
		return -1;

	return 0;
}

/* We allow "recursive" symbolic refs. Only within reason, though */
#define MAXDEPTH 5
#define MAXREFLEN (1024)

static const char *gitdb_resolve_ref_unsafe_locked(const char *dbprefix,
						   const char *refname,
						   int resolve_flags,
						   unsigned char *sha1,
						   int *flags)
{
  //FIXME  needs to be redone
	int depth = MAXDEPTH;
	static char refname_buffer[256];
	int bad_name = 0;
	struct strbuf data = STRBUF_INIT;

	if (flags)
		*flags = 0;

	if (check_refname_format(refname, REFNAME_ALLOW_ONELEVEL)) {
		if (flags)
			*flags |= REF_BAD_NAME;

		if (!(resolve_flags & RESOLVE_REF_ALLOW_BAD_NAME) ||
		    !refname_is_safe(refname)) {
			errno = EINVAL;
			return NULL;
		}
		/*
		 * dwim_ref() uses REF_ISBROKEN to distinguish between
		 * missing refs and refs that were present but invalid,
		 * to complain about the latter to stderr.
		 *
		 * We don't know whether the ref exists, so don't set
		 * REF_ISBROKEN yet.
		 */
		bad_name = 1;
	}

	for (;;) {
		char *buf;

		if (--depth < 0) {
			errno = ELOOP;
			goto failed;
		}

		data = fetch_refname(dbprefix, REFS_DB, refname);
		if (!data.len) {
		  if (! (resolve_flags & RESOLVE_REF_READING)) {
				hashclr(sha1);
				return refname;
			}
			errno = ENOENT;
			goto failed;
		}

		if (!starts_with((char *)data.buf, "ref:")) {
			/*
			 * Please note that FETCH_HEAD has a second
			 * line containing other data.
			 */
			if (get_sha1_hex((char *)data.buf, sha1) ||
			    (data.buf[40] != '\0' &&
			     !isspace(data.buf[40]))) {
				if (flags)
					*flags |= REF_ISBROKEN;
				if (resolve_flags & RESOLVE_REF_ALLOW_BAD_SHA1) {
					hashclr(sha1);
					return refname;
				}
				errno = EINVAL;
				goto failed;
			}
			strbuf_release(&data);
			if (bad_name) {
				hashclr(sha1);
				if (flags)
					*flags |= REF_ISBROKEN;
			}
			return refname;
		}
		if (flags)
			*flags |= REF_ISSYMREF;
		memcpy(refname_buffer, data.buf + 4, data.len - 4);
		buf = refname_buffer;
		while (isspace(*buf))
			buf++;
		if (check_refname_format(buf, REFNAME_ALLOW_ONELEVEL)) {
			if (flags)
				*flags |= REF_ISBROKEN;
			errno = EINVAL;
			goto failed;
		}
		refname = buf;
		strbuf_release(&data);
	}

 failed:
	strbuf_release(&data);
	return NULL;
}

static const char *gitdb_resolve_ref_unsafe_prefix(const char *dbprefix,
		const char *refname, int resolve_flags, unsigned char *sha1,
		int *flags)
{
	const char *ret;
	struct strbuf err = STRBUF_INIT;
	struct transaction *transaction = NULL;

	/*
	 * We take out a transaction while resolving the ref just to make
	 * sure no one else will be updating the entries.
	 */
	if (!inside_transaction) {
		transaction = transaction_begin(&err);
		if (!transaction) {
			errno = EIO;
			error("%s", err.buf);
			strbuf_release(&err);
			return NULL;
		}
	}
	ret =  gitdb_resolve_ref_unsafe_locked(dbprefix, refname,
					       resolve_flags, sha1, flags);
	if (transaction)
		transaction_free(transaction);
	return ret;
}

static const char *gitdb_resolve_ref_unsafe(const char *refname,
		int resolve_flags, unsigned char *sha1, int *flags)
{
	return gitdb_resolve_ref_unsafe_prefix(db_repo_name, refname,
					       resolve_flags, sha1, flags);
}

static struct transaction *gitdb_transaction_begin(struct strbuf *err)
{
	if (DB_transaction_start(err))
		return NULL;
	inside_transaction++;
	return xcalloc(1, sizeof(struct transaction));
}

static void gitdb_transaction_free(struct transaction *transaction)
{
	DB_transaction_cancel();
	free(transaction);
	inside_transaction--;
}

static int gitdb_transaction_update_reflog(struct transaction *transaction,
					   const char *refname,
					   const unsigned char *new_sha1,
					   const unsigned char *old_sha1,
					   struct reflog_committer_info *ci,
					   const char *msg, int flags,
					   struct strbuf *err)
{
	struct strbuf buf = STRBUF_INIT;
	char sign = (ci->tz < 0) ? '-' : '+';
	int zone = (ci->tz < 0) ? (-ci->tz) : ci->tz;
	int ret;

	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: update_reflog called for transaction that is not "
		    "open");

	if (flags & REFLOG_TRUNCATE) {
		delete_reflog(refname);
		if (create_reflog(refname, err))
			return -1;
		return 0;
	}

	if (flags & REFLOG_COMMITTER_INFO_IS_VALID)
		strbuf_addf(&buf, "%s", ci->committer_info);
	else
		strbuf_addf(&buf, "%s %lu %c%04d", ci->id, ci->timestamp,
			    sign, zone);

	ret = log_ref_write(refname, old_sha1, new_sha1, buf.buf, msg, err);
	strbuf_release(&buf);
	return ret;
}

struct rename_reflog_cb {
	struct transaction *transaction;
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

static int gitdb_transaction_rename_reflog(struct transaction *transaction,
			      const char *oldrefname,
			      const char *newrefname,
			      struct strbuf *err)
{
	struct rename_reflog_cb cb;

	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: transaction_rename_reflog called for transaction "
		    "that is not open");

	cb.transaction = transaction;
	cb.refname = newrefname;
	cb.err = err;
	if (for_each_reflog_ent(oldrefname, rename_reflog_ent, &cb))
		die("BUG: failed to read old reflog in "
		    "transaction_rename_reflog");

	return 0;
}
static int gitdb_transaction_update_ref(struct transaction *transaction,
					const char *refname,
					const unsigned char *new_sha1,
					const unsigned char *old_sha1,
					int flags, int have_old,
					const char *msg,
					struct strbuf *err)
{
	unsigned char current_sha1[20];
	const char *orig_refname = refname;
	struct object *o;
	int type;
	struct strbuf data;
	int mustexist = (old_sha1 && !is_null_sha1(old_sha1));

	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: update called for transaction that is not open");

	if (have_old && !old_sha1)
		die("BUG: have_old is true but old_sha1 is NULL");

	if (check_refname_format(refname, REFNAME_ALLOW_ONELEVEL)) {
		errno = EINVAL;
		strbuf_addf(err, "invalid refname: %s", refname);
		return -1;
	}

	refname = gitdb_resolve_ref_unsafe_locked(db_repo_name, refname,
						  mustexist ?
						  RESOLVE_REF_READING : 0,
						  current_sha1, &type);
	if (!refname) {
		errno = EINVAL;
		strbuf_addf(err, "unable to resolve reference %s",
			orig_refname);
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}
	if (log_ref_write(refname, old_sha1, new_sha1, git_committer_info(0),
			  msg, err)) {
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}
	if (flags & REF_NODEREF) {
		refname = orig_refname;
	}
	if (have_old && hashcmp(current_sha1, old_sha1)) {
		errno = EINVAL;
		strbuf_addf(err, "Ref %s is at %s but expected %s", refname,
			sha1_to_hex(current_sha1), sha1_to_hex(old_sha1));
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}

	if (is_null_sha1(new_sha1)) {
		if (delete_refname(REFS_DB, refname, err)) {
			transaction->state = TRANSACTION_ERROR;
			return -1;
		}
		if (delete_reflog(refname)) {
			transaction->state = TRANSACTION_ERROR;
			return -1;
		}
		return 0;
	}

	o = parse_object(new_sha1);
	if (!o) {
		errno = EINVAL;
		strbuf_addf(err, "Trying to write ref %s with "
			    "nonexistent object %s",
			    refname, sha1_to_hex(new_sha1));
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}
	if (o->type != OBJ_COMMIT && is_branch(refname)) {
		errno = EINVAL;
		strbuf_addf(err, "Trying to write non-commit object "
			    "%s to branch %s",
			    sha1_to_hex(new_sha1), refname);
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}

	data.buf = (char *)sha1_to_hex(new_sha1);
	data.len = 41;
	if (store_refname(REFS_DB, refname, &data, err)) {
		transaction->state = TRANSACTION_ERROR;
		return -1;
	}

	return 0;
}

static int gitdb_transaction_create_ref(struct transaction *transaction,
					const char *refname,
					const unsigned char *new_sha1,
					int flags, const char *msg,
					struct strbuf *err)
{
	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: create called for transaction that is not open");

	if (!new_sha1 || is_null_sha1(new_sha1))
		die("BUG: create ref with null new_sha1");

	return transaction_update_ref(transaction, refname, new_sha1,
				      null_sha1, flags, 1, msg, err);
}

static int gitdb_transaction_delete_ref(struct transaction *transaction,
					const char *refname,
					const unsigned char *old_sha1,
					int flags, int have_old,
					const char *msg,
					struct strbuf *err)
{
	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: delete called for transaction that is not open");

	if (have_old && !old_sha1)
		die("BUG: have_old is true but old_sha1 is NULL");

	if (have_old && is_null_sha1(old_sha1))
		die("BUG: have_old is true but old_sha1 is null_sha1");

	return transaction_update_ref(transaction, refname, null_sha1,
				      old_sha1, flags, have_old, msg, err);
}

static int gitdb_transaction_commit(struct transaction *transaction,
				     struct strbuf *err)
{
	if (transaction->state != TRANSACTION_OPEN)
		die("BUG: commit called for transaction that is not open");

	if (DB_transaction_commit(transaction, err))
		return -1;

	return 0;
}

static int show_one_reflog_ent(struct strbuf *data, each_reflog_ent_fn fn, void *cb_data)
{
	char *email_end, *message;
	unsigned long timestamp;
	int tz;

	/* Has to have at least 2 sha1 and two \0 */
	if (data->len < 42)
		return 0;
	/* ends with \0 */
	if (data->buf[data->len - 1])
		return 0;

	email_end = strchr(data->buf + 40, '>');
	if (!email_end)
		return 0;
	if (email_end[1] != ' ')
		return 0;
	timestamp = strtoul(email_end + 2, &message, 10);
	if (!timestamp)
		return 0;
	if (!message || message[0] != ' ')
		return 0;
	if (message[1] != '+' && message[1] != '-')
		return 0;
	if(!isdigit(message[2]) || !isdigit(message[3]) ||
	   !isdigit(message[4]) || !isdigit(message[5]))
		return 0;
	email_end[1] = '\0';
	tz = strtol(message + 1, NULL, 10);

	message += strlen(message) + 1;
	return fn((unsigned char *)data->buf, (unsigned char *)data->buf + 20,
		  data->buf + 40, timestamp, tz, message, cb_data);
}

static int gitdb_for_each_reflog_ent(const char *refname,
				   each_reflog_ent_fn fn,
				   void *cb_data)
{
	struct strbuf data = STRBUF_INIT;
	int ret = 0, i;

	data = fetch_refname(db_repo_name, LOGS_DB, refname);
	if (!data.len)
		return 0;
	if (data.len % sizeof(uint64_t)) {
		strbuf_release(&data);
		return 0;
	}

	for (i = 0; i < data.len; i += sizeof(uint64_t)) {
		struct strbuf key;
		struct strbuf refdata = STRBUF_INIT;

		key.buf = data.buf + i;
		key.len = sizeof(uint64_t);

		if (!*(uint64_t *)key.buf)
			continue;

		refdata = DB_fetch(LOGS_DB, &key);
		if (refdata.len)
			ret = show_one_reflog_ent(&refdata, fn, cb_data);
		strbuf_release(&refdata);
		if (ret)
			break;
	}
	strbuf_release(&data);
	return ret;
}

static int gitdb_for_each_reflog_ent_reverse(const char *refname,
					   each_reflog_ent_fn fn,
					   void *cb_data)
{
	struct strbuf data = STRBUF_INIT;
	int ret = 0, i;

	data = fetch_refname(db_repo_name, LOGS_DB, refname);
	if (!data.len)
		return 0;
	if (data.len % sizeof(uint64_t)) {
		strbuf_release(&data);
		return 0;
	}

	for (i = (data.len - 1) & (sizeof(uint64_t) -1);
	     i >= 0;
	     i -= sizeof(uint64_t)) {
		struct strbuf key;
		struct strbuf refdata = STRBUF_INIT;

		key.buf = data.buf + i;
		key.len = sizeof(uint64_t);

		if (!*(uint64_t *)key.buf)
			continue;

		refdata = DB_fetch(LOGS_DB, &key);
		if (refdata.len)
			ret = show_one_reflog_ent(&refdata, fn, cb_data);
		strbuf_release(&refdata);
		if (ret)
			break;
	}
	strbuf_release(&data);
	return ret;
}

static int gitdb_reflog_exists(const char *refname)
{
	struct strbuf data;
	data = fetch_refname(db_repo_name, LOGS_DB, refname);
	if (!data.len)
		return 0;
	strbuf_release(&data);
	return 1;
}

static int gitdb_create_reflog(const char *refname, struct strbuf *err)
{
	struct strbuf key = STRBUF_INIT;
	struct strbuf data;
	uint64_t zero = 0;
	int ret;

	strbuf_addf(&key, "%s%c%s%c", db_repo_name, '\0', refname, '\0');

	data.buf = (char *)&zero;
	data.len = sizeof(zero);

	ret = DB_store(LOGS_DB, &key, &data, err);

	strbuf_release(&key);
	return ret;
}

static int gitdb_delete_reflog(const char *refname)
{
	struct strbuf data;
	struct strbuf err = STRBUF_INIT;
	int i;

	data = fetch_refname(db_repo_name, LOGS_DB, refname);
	if (data.len % sizeof(uint64_t)) {
		error("%s", "reflog data size is not a multiple of 8");
		strbuf_release(&data);
		return -1;
	}
	for (i = 0; i < data.len; i += sizeof(uint64_t)) {
		struct strbuf key;

		if (*(uint64_t *)data.buf)
			continue;

		key.buf = data.buf + i;
		key.len = sizeof(uint64_t);

		DB_delete(LOGS_DB, &key, NULL);
	}
	strbuf_release(&data);

	if (delete_refname(LOGS_DB, refname, &err)) {
		error("%s", err.buf);
		strbuf_release(&err);
		return -1;
	}
	return 0;
}

struct reflog_cb {
	each_ref_fn *fn;
	void *cb_data;
};

static int do_one_reflog(struct strbuf *key, struct strbuf *unused,
			 void *private_data)
{
	struct reflog_cb *data = private_data;
	unsigned char sha1[20];
	const char *str = (char *)key->buf;

	/* First string is the repo name */
	if (strcmp(db_repo_name, str))
		return 0;

	str += strlen(str) + 1;

	/* Don't iterate over the magic refs, i.e. the onces that live
	   directly under .git
	*/
	if (!strchr((char *)str, '/'))
		return 0;

	if (read_ref_full(str, 0, sha1, NULL))
		return error("bad ref for %s", str);
	else
		return data->fn((char *)str, sha1, 0, data->cb_data);
}

static int gitdb_for_each_reflog(each_ref_fn fn, void *cb_data)
{
	struct reflog_cb data;
	struct strbuf err = STRBUF_INIT;

	data.fn = fn;
	data.cb_data = cb_data;

	if (DB_traverse_read(db_repo_name, LOGS_DB, do_one_reflog, NULL,
			     &data, &err) == -1) {
		error("%s", err.buf);
		strbuf_release(&err);
		return -1;
	}

	return 0;
}

/*
 * Return true iff the reference described by entry can be resolved to
 * an object in the database.  Emit a warning if the referred-to
 * object does not exist.
 */
static int ref_resolves_to_object(const char *refname,
				  const unsigned char *sha1)
{
	if (!has_sha1_file(sha1)) {
		error("%s does not point to a valid object!", refname);
		return 0;
	}
	return 1;
}

/* Include broken references in a do_for_each_ref*() iteration: */
#define DO_FOR_EACH_INCLUDE_BROKEN 0x01

struct ref_entry_cb {
	const char *base;
	int trim;
	int flags;
	each_ref_fn *fn;
	void *cb_data;
};

static int do_one_ref(struct strbuf *key, struct strbuf *data,
		      void *private_data)
{
	struct ref_entry_cb *cb_data = private_data;
	unsigned char sha1[20];
	int flag = 0;
	const char *str = (char *)key->buf;


	/* First string is the repo name */
	if (!starts_with(str, db_repo_name))
		return 0;

	str += strlen(str) + 1;

	/* Don't iterate over the magic refs, i.e. the onces that live
	   directly under .git
	*/
	if (!strchr((char *)str, '/'))
		return 0;

	if (!starts_with((char *)str, cb_data->base))
		return 0;

	/* all above checked in refsd traverse */
	if (starts_with((char *)data->buf, "ref:"))
		flag |= REF_ISSYMREF;
	else if (get_sha1_hex((char *)data->buf, sha1) < 0)
		return -1;

	if (!(cb_data->flags & DO_FOR_EACH_INCLUDE_BROKEN) &&
	    !ref_resolves_to_object(str, sha1))
		return 0;

	return cb_data->fn((char *)str + cb_data->trim, sha1,
			   flag, cb_data->cb_data);
}

/*
 * Call fn for each reference in the specified ref_cache for which the
 * refname begins with base.  If trim is non-zero, then trim that many
 * characters off the beginning of each refname before passing the
 * refname to fn.  flags can be DO_FOR_EACH_INCLUDE_BROKEN to include
 * broken references in the iteration.  If fn ever returns a non-zero
 * value, stop the iteration and return that value; otherwise, return
 * 0.
 */
static int do_for_each_ref(const char *dbprefix, const char *base,
			   each_ref_fn fn, int trim, int flags, void *cb_data)
{
	struct ref_entry_cb data;
	struct strbuf err = STRBUF_INIT;

	data.base = base;
	data.trim = trim;
	data.flags = flags;
	data.fn = fn;
	data.cb_data = cb_data;

	if (DB_traverse_read(dbprefix, REFS_DB, do_one_ref, base,
			     &data, &err) == -1) {
		error("%s", err.buf);
		strbuf_release(&err);
		return -1;
	}
	return 0;
}

static int gitdb_for_each_replace_ref(each_ref_fn fn, void *cb_data)
{
	return do_for_each_ref(db_repo_name, "refs/replace/", fn, 13, 0,
			       cb_data);
}

static int gitdb_for_each_rawref(each_ref_fn fn, void *cb_data)
{
	return do_for_each_ref(db_repo_name, "", fn, 0,
			       DO_FOR_EACH_INCLUDE_BROKEN, cb_data);
}

static int gitdb_for_each_ref_in(const char *prefix, each_ref_fn fn,
				 void *cb_data)
{
	return do_for_each_ref(db_repo_name, prefix, fn, strlen(prefix), 0,
			       cb_data);
}

static int gitdb_for_each_ref(each_ref_fn fn, void *cb_data)
{
	return do_for_each_ref(db_repo_name, "", fn, 0, 0, cb_data);
}

struct name_conflict_cb {
	const char *refname;
	const char *conflicting_refname;
	struct string_list *skip;
};

static int names_conflict(const char *refname1, const char *refname2)
{
	for (; *refname1 && *refname1 == *refname2; refname1++, refname2++)
		;
	return (*refname1 == '\0' && *refname2 == '/')
		|| (*refname1 == '/' && *refname2 == '\0');
}

static int name_conflic_fn(const char *refname,	const unsigned char *sha1,
			   int flags, void *cb_data)
{
	struct name_conflict_cb *data = cb_data;

	if (string_list_has_string(data->skip, refname))
		return 0;
	if (names_conflict(data->refname, refname)) {
		data->conflicting_refname = refname;
		return 1;
	}
	return 0;
}

static int gitdb_is_refname_available(const char *refname,
				      struct string_list *skip)
{
	struct name_conflict_cb cb_data;

	cb_data.refname = refname;
	cb_data.conflicting_refname = NULL;
	cb_data.skip = skip;

	if (do_for_each_ref(db_repo_name, "", name_conflic_fn, 0,
			    DO_FOR_EACH_INCLUDE_BROKEN, &cb_data)) {
		error("'%s' exists; cannot create '%s'",
		      cb_data.conflicting_refname, refname);
		return 0;
	}
	return 1;
}

enum peel_status {
	/* object was peeled successfully: */
	PEEL_PEELED = 0,

	/*
	 * object cannot be peeled because the named object (or an
	 * object referred to by a tag in the peel chain), does not
	 * exist.
	 */
	PEEL_INVALID = -1,

	/* object cannot be peeled because it is not a tag: */
	PEEL_NON_TAG = -2,

	/* ref_entry contains no peeled value because it is a symref: */
	PEEL_IS_SYMREF = -3,

	/*
	 * ref_entry cannot be peeled because it is broken (i.e., the
	 * symbolic reference cannot even be resolved to an object
	 * name):
	 */
	PEEL_BROKEN = -4
};

/*
 * Peel the named object; i.e., if the object is a tag, resolve the
 * tag recursively until a non-tag is found.  If successful, store the
 * result to sha1 and return PEEL_PEELED.  If the object is not a tag
 * or is not valid, return PEEL_NON_TAG or PEEL_INVALID, respectively,
 * and leave sha1 unchanged.
 */
static enum peel_status peel_object(const unsigned char *name, unsigned char *sha1)
{
	struct object *o = lookup_unknown_object(name);

	if (o->type == OBJ_NONE) {
		int type = sha1_object_info(name, NULL);
		if (type < 0)
			return PEEL_INVALID;
		o->type = type;
	}

	if (o->type != OBJ_TAG)
		return PEEL_NON_TAG;

	o = deref_tag_noverify(o);
	if (!o)
		return PEEL_INVALID;

	hashcpy(sha1, o->sha1);
	return PEEL_PEELED;
}

static int gitdb_peel_ref(const char *refname, unsigned char *sha1)
{
	int flag;
	unsigned char base[20];

	if (read_ref_full(refname, RESOLVE_REF_READING, base, &flag)) {
		return -1;
	}

	return peel_object(base, sha1);
}

int gitdb_config(const char *var, const char *value, void *ptr)
{
	struct gitdb_config_data *cdata = ptr;

	if (!strcmp(var, "core.db-repo-name"))
		cdata->db_repo_name = strdup((char *)value);
	if (!strcmp(var, "core.db-socket"))
		cdata->db_socket = strdup((char *)value);
	if (!strcmp(var, "core.refs-backend-type"))
		cdata->refs_backend_type = strdup((char *)value);
	return 0;
}

/*
 * Verifies that the gitlink points to a compatible repository
 * (a repository sharing the same db-socket) and returns the
 * repository name, or NULL if there is an error.
 */
static char *get_submodule_repo_name(const char *path)
{
	int len = strlen(path);
	char *submodule;
	char *cfgpath;
	struct gitdb_config_data cdata = {NULL, NULL};

	while (len && path[len-1] == '/')
		len--;
	if (!len)
		return NULL;

	submodule = xstrndup(path, len);
	cfgpath = *submodule
		? git_path_submodule(submodule, "config")
		: git_path("config");
	free(submodule);

	if (git_config_from_file(gitdb_config, cfgpath, &cdata))
		return NULL;

	if (strcmp(db_socket, cdata.db_socket))
		die("gitlink in %s points to a repo with a different backend",
		    cfgpath);

	free(cdata.db_socket);
	return cdata.db_repo_name;
}

static int gitdb_resolve_gitlink_ref(const char *path, const char *refname,
				     unsigned char *sha1)
{
	int retval = 0;
	char *reponame;

	reponame = get_submodule_repo_name(path);
	if (!reponame)
		return -1;

	if (!gitdb_resolve_ref_unsafe_prefix(reponame, refname,
					     RESOLVE_REF_READING, sha1, NULL))
		retval = -1;

	free(reponame);
	return retval;
}

static int do_head_ref(const char *submodule, each_ref_fn fn, void *cb_data)
{
	unsigned char sha1[20];
	int flag;

	if (submodule) {
		if (resolve_gitlink_ref(submodule, "HEAD", sha1) == 0)
			return fn("HEAD", sha1, 0, cb_data);

		return 0;
	}

	if (!read_ref_full("HEAD", RESOLVE_REF_READING, sha1, &flag))
		return fn("HEAD", sha1, flag, cb_data);

	return 0;
}

static int gitdb_head_ref(each_ref_fn fn, void *cb_data)
{
	return do_head_ref(NULL, fn, cb_data);
}

static int gitdb_head_ref_submodule(const char *submodule, each_ref_fn fn,
				    void *cb_data)
{
	return do_head_ref(submodule, fn, cb_data);
}

static int gitdb_for_each_ref_submodule(const char *submodule, each_ref_fn fn,
				      void *cb_data)
{
	int ret = 0;
	char *reponame;

	if (!submodule || !*submodule)
		return do_for_each_ref(db_repo_name, "", fn, 0, 0, cb_data);

	reponame = get_submodule_repo_name(submodule);
	if (!reponame)
		return -1;

	ret = do_for_each_ref(reponame, "", fn, 0, 0, cb_data);
	free(reponame);
	return ret;
}

static int gitdb_for_each_ref_in_submodule(const char *submodule,
					 const char *prefix,
					 each_ref_fn fn, void *cb_data)
{
	int ret = 0;
	char *reponame;

	if (!submodule || !*submodule)
		return do_for_each_ref(db_repo_name, prefix, fn,
				       strlen(prefix), 0, cb_data);

	reponame = get_submodule_repo_name(submodule);
	if (!reponame)
		return -1;

	ret = do_for_each_ref(reponame, prefix, fn, strlen(prefix), 0, cb_data);
	free(reponame);
	return ret;
}

static int gitdb_for_each_namespaced_ref(each_ref_fn fn, void *cb_data)
{
	struct strbuf buf = STRBUF_INIT;
	int ret;
	strbuf_addf(&buf, "%srefs/", get_git_namespace());
	ret = do_for_each_ref(db_repo_name, buf.buf, fn, 0, 0, cb_data);
	strbuf_release(&buf);
	return ret;
}

static void gitdb_init_backend(void)
{
	struct sockaddr_un addr;

	if (dbs != -1)
		return;

	dbs = socket(AF_UNIX, SOCK_STREAM, 0);
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, db_socket, sizeof(addr.sun_path));

	if (connect(dbs, (struct sockaddr *)&addr, sizeof(addr)) == -1)
		die("failed to connect to db socket %s %s", db_socket,
		    strerror(errno));
}

struct ref_be refs_be_db = {
	NULL,
	"db",
	gitdb_init_backend,
	gitdb_transaction_begin,
	gitdb_transaction_update_ref,
	gitdb_transaction_create_ref,
	gitdb_transaction_delete_ref,
	gitdb_transaction_update_reflog,
	gitdb_transaction_rename_reflog,
	gitdb_transaction_commit,
	gitdb_transaction_free,
	gitdb_for_each_reflog_ent,
	gitdb_for_each_reflog_ent_reverse,
	gitdb_for_each_reflog,
	gitdb_reflog_exists,
	gitdb_create_reflog,
	gitdb_delete_reflog,
	gitdb_resolve_ref_unsafe,
	gitdb_is_refname_available,
	gitdb_pack_refs,
	gitdb_peel_ref,
	gitdb_create_symref,
	gitdb_resolve_gitlink_ref,
	gitdb_head_ref,
	gitdb_head_ref_submodule,
	gitdb_for_each_ref,
	gitdb_for_each_ref_submodule,
	gitdb_for_each_ref_in,
	gitdb_for_each_ref_in_submodule,
	gitdb_for_each_rawref,
	gitdb_for_each_namespaced_ref,
	gitdb_for_each_replace_ref,
};
