#ifndef _ISVN_ISVN_INTERNAL_H_
#define _ISVN_ISVN_INTERNAL_H_

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"

/* Fixup later: */
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* This one is stupid. Don't "fix." */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

/* XXX: 8+ */
#define NR_WORKERS 1
/* XXX: 25? */
#define REV_CHUNK 25

#ifndef __DECONST
#define __DECONST(p, t) ((t)(uintptr_t)(const void *)(p))
#endif

/* Edit flags: */
#define ED_DELETE	0x10000000
#define ED_FLAGMASK	0xf0000000

/* Edit types: */
#define ED_TYPEMASK	0x0fffffff
enum ed_type {
	ED_NIL = 0,
	ED_MKDIR,
	ED_ADDFILE,
	ED_TEXTDELTA,
	ED_PROP,
};

/* There are potentially many edits per revision. An edit describes a change to
 * one file or directory. */
/* NOTE SVN directory deletes are recursive, must be processed before adds on
 * subdirectories/subfiles (which can happen in the same rev. */
struct br_edit {
	struct hashmap_entry	e_entry;
	/* Edits are complicated, need to be applied in the same order... */
	TAILQ_ENTRY(br_edit)	e_list;

	char		*e_path;	/* (hash key) the path of this change */
	unsigned	 e_kind;	/* enum ed_typem | ED_FLAGS */
	unsigned short	 e_fmode;	/* new (SVN) mode (mkdir/addfile/prop) */

	char		*e_copyfrom;
	unsigned	 e_copyrev;

	char		*e_diff;
	size_t		 e_difflen;

	unsigned char	 e_preimage_md5[16];
	unsigned char	 e_postimage_md5[16];

	unsigned char	 e_old_sha1[20];
	unsigned char	 e_new_sha1[20];
};

/* A logical SVN revision. */
struct branch_rev {
	TAILQ_ENTRY(branch_rev)		rv_list;

	unsigned			rv_rev;
	struct hashmap			rv_edits;
	TAILQ_HEAD(, br_edit)		rv_editorder;

	unsigned char			rv_parent[20];

	/* Normally commits will only touch one branch. A common exception is
	 * r1, when the SVN user populates the tree structure. We'll do some
	 * verification and assume a revision only touches one branch for
	 * non-empty (in the git sense -- directory-only) commits. */
	bool				rv_only_empty_dirs;
	const char			*rv_branch;

	char				*rv_author;
	char				*rv_logmsg;

	/* This is real ugly, but SVN revs *can* touch multiple git branches
	 * (e.g., 'svn mv branch1/ branch2/'. */
	struct branch_rev		*rv_affil;
	bool				rv_secondary;

	unsigned long			rv_timestamp;
};

struct isvn_client_ctx {
	apr_pool_t		*svn_pool;
	svn_client_ctx_t	*svn_client;
	svn_ra_session_t	*svn_session;
};

struct svn_branch;

extern int option_verbosity;
extern char *option_origin;
extern const char *option_trunk;
extern const char *option_branches;
extern const char *option_tags;
extern const char *option_user, *option_password;
extern const char *g_svn_url;
extern const char *g_repos_root;
extern apr_pool_t *g_apr_pool;

void isvn_g_lock(void);
void isvn_g_unlock(void);
void isvn_fetcher_getrange(unsigned *revlo, unsigned *revhi, bool *done);
void isvn_mark_fetchdone(unsigned revlo, unsigned revhi);
/* Blocks until all revs up to 'rev' are fetched. */
void isvn_wait_fetch(unsigned rev);
bool isvn_all_fetched(void);

void isvn_editor_init(void);
void isvn_editor_inialize_dedit_obj(svn_delta_editor_t *de);
bool path_startswith(const char *haystack, const char *needle);
const char *strip_branch(const char *path, const char **branch_out);

void isvn_fetch_init(void);
void *isvn_fetch_worker(void *dummy_i);
struct isvn_client_ctx *get_svn_ctx(void);
void assert_noerr(svn_error_t *err, const char *fmt, ...);
void assert_status_noerr(apr_status_t status, const char *fmt, ...);
void branch_edit_free(struct br_edit *edit);
struct branch_rev *new_branch_rev(svn_revnum_t rev);
void branch_rev_free(struct branch_rev *br);

void isvn_brancher_init(void);
struct svn_branch *svn_branch_get(struct hashmap *h, const char *name);
void svn_branch_hash_init(struct hashmap *hash);
void svn_branch_revs_enqueue_and_free(struct svn_branch *branch);
void svn_branch_append(struct svn_branch *sb, struct branch_rev *br);
void *isvn_bucket_worker(void *v);

void isvn_revmap_init(void);
void isvn_revmap_insert(unsigned revnum, const char *branch,
	unsigned char sha1[20]);
void isvn_revmap_lookup(unsigned revnum, const char **branch_out,
	unsigned char sha1_out[20]);
/* Scans from 'rev' downward looking for 'branch'.
 * The usual negative return => error (ENOENT).
 * Sets 'sha1_out' to the hash of the commit if found (no error). */
int isvn_revmap_lookup_branchlatest(const char *branch, unsigned rev,
	unsigned char sha1_out[20]);
void isvn_dump_revmap(void);
void isvn_assert_commit(const char *branch, unsigned rev);
bool isvn_has_commit(const char *branch, unsigned rev);

static inline bool startswith(const char *haystack, const char *needle)
{
	return (strncmp(haystack, needle, strlen(needle)) == 0);
}

static inline void mtx_init(pthread_mutex_t *lk)
{
	int rc;

	rc = pthread_mutex_init(lk, NULL);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void mtx_lock(pthread_mutex_t *lk)
{
	int rc;

	rc = pthread_mutex_lock(lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void mtx_unlock(pthread_mutex_t *lk)
{
	int rc;

	rc = pthread_mutex_unlock(lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void cond_init(pthread_cond_t *cond)
{
	int rc;

	rc = pthread_cond_init(cond, NULL);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void cond_wait(pthread_cond_t *cond, pthread_mutex_t *lk)
{
	int rc;

	rc = pthread_cond_wait(cond, lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void cond_broadcast(pthread_cond_t *cond)
{
	int rc;

	rc = pthread_cond_broadcast(cond);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void rw_init(pthread_rwlock_t *lk)
{
	int rc;

	rc = pthread_rwlock_init(lk, NULL);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void rw_wlock(pthread_rwlock_t *lk)
{
	int rc;

	rc = pthread_rwlock_wrlock(lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void rw_rlock(pthread_rwlock_t *lk)
{
	int rc;

	rc = pthread_rwlock_rdlock(lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

static inline void rw_unlock(pthread_rwlock_t *lk)
{
	int rc;

	rc = pthread_rwlock_unlock(lk);
	if (rc)
		die("%s: %s(%d)\n", __func__, strerror(rc), rc);
}

/* Like CONCAT, but only removes elements in head2 after last2 */
#define TAILQ_SPLICE(head1, head2, last2, field) do {			\
	if (TAILQ_NEXT((last2), field)) {				\
		*(head1)->tqh_last = TAILQ_NEXT((last2), field);	\
		TAILQ_NEXT((last2), field)->field.tqe_prev =		\
		    (head1)->tqh_last;					\
		(head1)->tqh_last = (head2)->tqh_last;			\
		TAILQ_NEXT((last2), field) = NULL;			\
		(last2)->field.tqe_prev = &TAILQ_FIRST((head2));	\
		(head2)->tqh_last = &TAILQ_NEXT((last2), field);	\
	}								\
} while (0)

/* Missing in Linux sys/queue.h */
#ifndef TAILQ_SWAP
#define TAILQ_SWAP(head1, head2, type, field) do {			\
	struct type *swap_first = (head1)->tqh_first;			\
	struct type **swap_last = (head1)->tqh_last;			\
	(head1)->tqh_first = (head2)->tqh_first;			\
	(head1)->tqh_last = (head2)->tqh_last;				\
	(head2)->tqh_first = swap_first;				\
	(head2)->tqh_last = swap_last;					\
	if ((swap_first = (head1)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head1)->tqh_first;	\
	else								\
		(head1)->tqh_last = &(head1)->tqh_first;		\
	if ((swap_first = (head2)->tqh_first) != NULL)			\
		swap_first->field.tqe_prev = &(head2)->tqh_first;	\
	else								\
		(head2)->tqh_last = &(head2)->tqh_first;		\
} while (0)
#endif

/* Missing in Linux sys/queue.h despite being in QUEUE(3) linux manpage. */
#ifndef TAILQ_FOREACH_SAFE
#define	TAILQ_FOREACH_SAFE(var, head, field, tvar)			\
	for ((var) = TAILQ_FIRST((head));				\
	    (var) && ((tvar) = TAILQ_NEXT((var), field), 1);		\
	    (var) = (tvar))
#endif

#ifndef __unused
#define __unused __attribute__((unused))
#endif

#endif
