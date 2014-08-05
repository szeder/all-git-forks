#ifndef REFS_H
#define REFS_H

struct ref_lock {
	char *ref_name;
	char *orig_ref_name;
	struct lock_file *lk;
	unsigned char old_sha1[20];
	int lock_fd;
	int force_write;
};

/*
 * A ref_transaction represents a collection of ref updates
 * that should succeed or fail together.
 *
 * Calling sequence
 * ----------------
 * - Allocate and initialize a `struct ref_transaction` by calling
 *   `transaction_begin()`.
 *
 * - List intended ref updates by calling functions like
 *   `transaction_update_sha1()` and `transaction_create_sha1()`.
 *
 * - Call `transaction_commit()` to execute the transaction.
 *   If this succeeds, the ref updates will have taken place and
 *   the transaction cannot be rolled back.
 *
 * - At any time call `transaction_free()` to discard the
 *   transaction and free associated resources.  In particular,
 *   this rolls back the transaction if it has not been
 *   successfully committed.
 *
 * Error handling
 * --------------
 *
 * On error, transaction functions append a message about what
 * went wrong to the 'err' argument.  The message mentions what
 * ref was being updated (if any) when the error occurred so it
 * can be passed to 'die' or 'error' as-is.
 *
 * The message is appended to err without first clearing err.
 * err will not be '\n' terminated.
 */
struct ref_transaction;

/*
 * Bit values set in the flags argument passed to each_ref_fn():
 */

/* Reference is a symbolic reference. */
#define REF_ISSYMREF 0x01

/* Reference is a packed reference. */
#define REF_ISPACKED 0x02

/*
 * Reference cannot be resolved to an object name: dangling symbolic
 * reference (directly or indirectly), corrupt reference file, or
 * symbolic reference refers to ill-formatted reference name.
 */
#define REF_ISBROKEN 0x04

/*
 * The signature for the callback function for the for_each_*()
 * functions below.  The memory pointed to by the refname and sha1
 * arguments is only guaranteed to be valid for the duration of a
 * single callback invocation.
 */
typedef int each_ref_fn(const char *refname,
			const unsigned char *sha1, int flags, void *cb_data);

/*
 * The following functions invoke the specified callback function for
 * each reference indicated.  If the function ever returns a nonzero
 * value, stop the iteration and return that value.  Please note that
 * it is not safe to modify references while an iteration is in
 * progress, unless the same callback function invocation that
 * modifies the reference also returns a nonzero value to immediately
 * stop the iteration.
 */
extern int head_ref(each_ref_fn, void *);
extern int for_each_ref(each_ref_fn, void *);
extern int for_each_ref_in(const char *, each_ref_fn, void *);
extern int for_each_tag_ref(each_ref_fn, void *);
extern int for_each_branch_ref(each_ref_fn, void *);
extern int for_each_remote_ref(each_ref_fn, void *);
extern int for_each_replace_ref(each_ref_fn, void *);
extern int for_each_glob_ref(each_ref_fn, const char *pattern, void *);
extern int for_each_glob_ref_in(each_ref_fn, const char *pattern, const char* prefix, void *);

extern int head_ref_submodule(const char *submodule, each_ref_fn fn, void *cb_data);
extern int for_each_ref_submodule(const char *submodule, each_ref_fn fn, void *cb_data);
extern int for_each_ref_in_submodule(const char *submodule, const char *prefix,
		each_ref_fn fn, void *cb_data);
extern int for_each_tag_ref_submodule(const char *submodule, each_ref_fn fn, void *cb_data);
extern int for_each_branch_ref_submodule(const char *submodule, each_ref_fn fn, void *cb_data);
extern int for_each_remote_ref_submodule(const char *submodule, each_ref_fn fn, void *cb_data);

extern int head_ref_namespaced(each_ref_fn fn, void *cb_data);
extern int for_each_namespaced_ref(each_ref_fn fn, void *cb_data);

static inline const char *has_glob_specials(const char *pattern)
{
	return strpbrk(pattern, "?*[");
}

/* can be used to learn about broken ref and symref */
extern int for_each_rawref(each_ref_fn, void *);

extern void warn_dangling_symref(FILE *fp, const char *msg_fmt, const char *refname);
extern void warn_dangling_symrefs(FILE *fp, const char *msg_fmt, const struct string_list* refnames);

/*
 * Flags for controlling behaviour of pack_refs()
 * PACK_REFS_PRUNE: Prune loose refs after packing
 * PACK_REFS_ALL:   Pack _all_ refs, not just tags and already packed refs
 */
#define PACK_REFS_PRUNE 0x0001
#define PACK_REFS_ALL   0x0002

/*
 * Write a packed-refs file for the current repository.
 * flags: Combination of the above PACK_REFS_* flags.
 * Returns 0 on success and fills in err on failure.
 */
int pack_refs(unsigned int flags, struct strbuf *err);

extern int ref_exists(const char *);

/*
 * Return true iff refname1 and refname2 conflict with each other.
 * Two reference names conflict if one of them exactly matches the
 * leading components of the other; e.g., "foo/bar" conflicts with
 * both "foo" and with "foo/bar/baz" but not with "foo/bar" or
 * "foo/barbados".
 */
int names_conflict(const char *refname1, const char *refname2);

extern int is_branch(const char *refname);

/*
 * Check is a particular refname is available for creation. skip contains
 * a list of refnames to exclude from the refname collission tests.
 */
int is_refname_available(const char *refname, const char **skip, int skipnum);

/*
 * If refname is a non-symbolic reference that refers to a tag object,
 * and the tag can be (recursively) dereferenced to a non-tag object,
 * store the SHA1 of the referred-to object to sha1 and return 0.  If
 * any of these conditions are not met, return a non-zero value.
 * Symbolic references are considered unpeelable, even if they
 * ultimately resolve to a peelable tag.
 */
extern int peel_ref(const char *refname, unsigned char *sha1);

/*
 * Flags controlling transaction_update_sha1(), transaction_create_sha1(), etc.
 * REF_NODEREF: act on the ref directly, instead of dereferencing
 *              symbolic references.
 * REF_ALLOWBROKEN: allow locking refs that are broken.
 * Flags >= 0x100 are reserved for internal use.
 */
#define REF_NODEREF	0x01
#define REF_ALLOWBROKEN 0x02

/** Reads log for the value of ref during at_time. **/
extern int read_ref_at(const char *refname, unsigned long at_time, int cnt,
		       unsigned char *sha1, char **msg,
		       unsigned long *cutoff_time, int *cutoff_tz, int *cutoff_cnt);

/** Check if a particular reflog exists */
extern int reflog_exists(const char *refname);

/** Create reflog. Set errno to something meaningful on failure. */
extern int create_reflog(const char *refname);

/** Delete a reflog */
extern int delete_reflog(const char *refname);

/* iterate over reflog entries */
typedef int each_reflog_ent_fn(unsigned char *osha1, unsigned char *nsha1, const char *, unsigned long, int, const char *, void *);
int for_each_reflog_ent(const char *refname, each_reflog_ent_fn fn, void *cb_data);
int for_each_reflog_ent_reverse(const char *refname, each_reflog_ent_fn fn, void *cb_data);

/*
 * Calls the specified function for each reflog file until it returns nonzero,
 * and returns the value
 */
extern int for_each_reflog(each_ref_fn, void *);

#define REFNAME_ALLOW_ONELEVEL 1
#define REFNAME_REFSPEC_PATTERN 2
#define REFNAME_DOT_COMPONENT 4

/*
 * Return 0 iff refname has the correct format for a refname according
 * to the rules described in Documentation/git-check-ref-format.txt.
 * If REFNAME_ALLOW_ONELEVEL is set in flags, then accept one-level
 * reference names.  If REFNAME_REFSPEC_PATTERN is set in flags, then
 * allow a "*" wildcard character in place of one of the name
 * components.  No leading or repeated slashes are accepted.  If
 * REFNAME_DOT_COMPONENT is set in flags, then allow refname
 * components to start with "." (but not a whole component equal to
 * "." or "..").
 */
extern int check_refname_format(const char *refname, int flags);

extern const char *prettify_refname(const char *refname);
extern char *shorten_unambiguous_ref(const char *refname, int strict);

/** rename ref, return 0 on success **/
extern int rename_ref(const char *oldref, const char *newref, const char *logmsg);

/**
 * Resolve refname in the nested "gitlink" repository that is located
 * at path.  If the resolution is successful, return 0 and set sha1 to
 * the name of the object; otherwise, return a non-zero value.
 */
extern int resolve_gitlink_ref(const char *path, const char *refname, unsigned char *sha1);

/*
 * Begin a reference transaction.  The reference transaction must
 * be freed by calling transaction_free().
 */
struct ref_transaction *transaction_begin(struct strbuf *err);

/*
 * The following functions add a reference check or update to a
 * ref_transaction.  In all of them, refname is the name of the
 * reference to be affected.  The functions make internal copies of
 * refname, so the caller retains ownership of the parameter.  flags
 * can be REF_NODEREF; it is passed to update_ref_lock().
 */

/*
 * Add a reference update to transaction.  new_sha1 is the value that
 * the reference should have after the update, or null_sha1 if it should
 * be deleted.  If have_old is true and old_sha is not the null_sha1
 * then the previous value of the ref must match or the update will fail.
 * If have_old is true and old_sha1 is the null_sha1 then the ref must not
 * already exist and a new ref will be created with new_sha1.
 * Function returns 0 on success and non-zero on failure. A failure to update
 * means that the transaction as a whole has failed and will need to be
 * rolled back.
 */
int transaction_update_sha1(struct ref_transaction *transaction,
			    const char *refname,
			    const unsigned char *new_sha1,
			    const unsigned char *old_sha1,
			    int flags, int have_old, const char *msg,
			    struct strbuf *err);

/*
 * Add a reference creation to transaction.  new_sha1 is the value
 * that the reference should have after the update; it must not be the
 * null SHA-1.  It is verified that the reference does not exist
 * already.
 * Function returns 0 on success and non-zero on failure. A failure to create
 * means that the transaction as a whole has failed and will need to be
 * rolled back.
 */
int transaction_create_sha1(struct ref_transaction *transaction,
			    const char *refname,
			    const unsigned char *new_sha1,
			    int flags, const char *msg,
			    struct strbuf *err);

/*
 * Add a reference deletion to transaction.  If have_old is true, then
 * old_sha1 holds the value that the reference should have had before
 * the update (which must not be the null SHA-1).
 * Function returns 0 on success and non-zero on failure. A failure to delete
 * means that the transaction as a whole has failed and will need to be
 * rolled back.
 */
int transaction_delete_sha1(struct ref_transaction *transaction,
			    const char *refname,
			    const unsigned char *old_sha1,
			    int flags, int have_old, const char *msg,
			    struct strbuf *err);

/*
 * Flags controlling transaction_update_reflog().
 * REFLOG_TRUNCATE: Truncate the reflog.
 *
 * Flags >= 0x100 are reserved for internal use.
 */
#define REFLOG_TRUNCATE 0x01
#define REFLOG_COMMITTER_INFO_IS_VALID 0x02

/*
 * Committer data provided to reflog updates.
 * If flags contain REFLOG_COMMITTER_DATA_IS_VALID then
 * then the structure contains a prebaked committer string
 * just like git_committer_info() would return.
 *
 * If flags does not contain REFLOG_COMMITTER_DATA_IS_VALID
 * then the committer info string will be generated using the passed
 * email, timestamp and tz fields.
 * This is useful for example from reflog iterators where you are passed
 * these fields individually and not as a prebaked git_committer_info()
 * string.
 */
struct reflog_committer_info {
	const char *committer_info;

	const char *id;
	unsigned long timestamp;
	int tz;
};
/*
 * Append a reflog entry for refname. If the REFLOG_TRUNCATE flag is set
 * this update will first truncate the reflog before writing the entry.
 * If msg is NULL no update will be written to the log.
 */
int transaction_update_reflog(struct ref_transaction *transaction,
			      const char *refname,
			      const unsigned char *new_sha1,
			      const unsigned char *old_sha1,
			      struct reflog_committer_info *ci,
			      const char *msg, int flags,
			      struct strbuf *err);

/*
 * Commit all of the changes that have been queued in transaction, as
 * atomically as possible.  Return a nonzero value if there is a
 * problem.
 * If the transaction is already in failed state this function will return
 * an error.
 * Function returns 0 on success, -1 for generic failures and
 * UPDATE_REFS_NAME_CONFLICT (-2) if the failure was due to a name
 * collision (ENOTDIR).
 */
#define UPDATE_REFS_NAME_CONFLICT -2
int transaction_commit(struct ref_transaction *transaction,
		       struct strbuf *err);

/*
 * Free an existing transaction and all associated data.
 */
void transaction_free(struct ref_transaction *transaction);

/** Lock a ref and then write its file */
int update_ref(const char *action, const char *refname,
	       const unsigned char *sha1, const unsigned char *oldval,
	       int flags, struct strbuf *err);

extern int parse_hide_refs_config(const char *var, const char *value, const char *);
extern int ref_is_hidden(const char *);


/* refs backends */
typedef struct ref_transaction *(*transaction_begin_fn)(struct strbuf *err);
typedef int (*transaction_update_sha1_fn)(struct ref_transaction *transaction,
		const char *refname, const unsigned char *new_sha1,
		const unsigned char *old_sha1, int flags, int have_old,
		const char *msg, struct strbuf *err);
typedef int (*transaction_create_sha1_fn)(struct ref_transaction *transaction,
		const char *refname, const unsigned char *new_sha1,
		int flags, const char *msg, struct strbuf *err);
typedef int (*transaction_delete_sha1_fn)(struct ref_transaction *transaction,
		const char *refname, const unsigned char *old_sha1,
		int flags, int have_old, const char *msg, struct strbuf *err);
typedef int (*transaction_update_reflog_fn)(
		struct ref_transaction *transaction,
		const char *refname, const unsigned char *new_sha1,
		const unsigned char *old_sha1,
		struct reflog_committer_info *ci,
		const char *msg, int flags, struct strbuf *err);
typedef int (*transaction_commit_fn)(struct ref_transaction *transaction,
				       struct strbuf *err);
typedef void (*transaction_free_fn)(struct ref_transaction *transaction);

typedef int (*for_each_reflog_ent_fn)(const char *refname,
				      each_reflog_ent_fn fn,
				      void *cb_data);
typedef int (*for_each_reflog_ent_reverse_fn)(const char *refname,
					      each_reflog_ent_fn fn,
					      void *cb_data);
typedef int (*for_each_reflog_fn)(each_ref_fn fn, void *cb_data);
typedef int (*reflog_exists_fn)(const char *refname);
typedef int (*create_reflog_fn)(const char *refname);
typedef int (*delete_reflog_fn)(const char *refname);

typedef const char *(*resolve_ref_unsafe_fn)(const char *ref,
		unsigned char *sha1, int reading, int *flag);

typedef int (*is_refname_available_fn)(const char *refname, const char **skip,
				       int skipnum);
typedef int (*pack_refs_fn)(unsigned int flags, struct strbuf *err);
typedef int (*peel_ref_fn)(const char *refname, unsigned char *sha1);
typedef int (*create_symref_fn)(const char *ref_target,
				const char *refs_heads_master,
				const char *logmsg);
typedef int (*resolve_gitlink_ref_fn)(const char *path, const char *refname,
				      unsigned char *sha1);

typedef int (*head_ref_fn)(each_ref_fn fn, void *cb_data);
typedef int (*head_ref_submodule_fn)(const char *submodule, each_ref_fn fn,
				     void *cb_data);
typedef int (*head_ref_namespaced_fn)(each_ref_fn fn, void *cb_data);

struct ref_be {
	transaction_begin_fn transaction_begin;
	transaction_update_sha1_fn transaction_update_sha1;
	transaction_create_sha1_fn transaction_create_sha1;
	transaction_delete_sha1_fn transaction_delete_sha1;
	transaction_update_reflog_fn transaction_update_reflog;
	transaction_commit_fn transaction_commit;
	transaction_free_fn transaction_free;

	for_each_reflog_ent_fn for_each_reflog_ent;
	for_each_reflog_ent_reverse_fn for_each_reflog_ent_reverse;
	for_each_reflog_fn for_each_reflog;
	reflog_exists_fn reflog_exists;
	create_reflog_fn create_reflog;
	delete_reflog_fn delete_reflog;

	resolve_ref_unsafe_fn resolve_ref_unsafe;
	is_refname_available_fn is_refname_available;
	pack_refs_fn pack_refs;
	peel_ref_fn peel_ref;
	create_symref_fn create_symref;
	resolve_gitlink_ref_fn resolve_gitlink_ref;

	head_ref_fn head_ref;
	head_ref_submodule_fn head_ref_submodule;
	head_ref_namespaced_fn head_ref_namespaced;
};

extern struct ref_be *refs;

#endif /* REFS_H */
