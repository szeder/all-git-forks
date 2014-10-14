#ifndef _ISVN_ISVN_INTERNAL_H_
#define _ISVN_ISVN_INTERNAL_H_

/* Ideally, ifndef <BSDLIBC>. But I don't see an appropriate define. */
#ifdef __GLIBC__
#include <bsd/sys/queue.h>
#include <bsd/string.h>
#else
#include <sys/queue.h>
#endif

#pragma GCC diagnostic error "-Wall"
#pragma GCC diagnostic error "-Wextra"

/* Fixup later: */
#pragma GCC diagnostic ignored "-Wunused-parameter"

/* This one is stupid. Don't "fix." */
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "isvn/isvn-hashmap.h"

extern unsigned g_nr_fetch_workers;
extern unsigned g_nr_commit_workers;
extern unsigned g_rev_chunk;

#ifndef __DECONST
#define __DECONST(p, t) ((t)(uintptr_t)(const void *)(p))
#endif
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

/* Bizarrely, libbsd is missing LIST_PREV and SLIST_SWAP */
#ifndef SLIST_SWAP
#define SLIST_SWAP(head1, head2, type) do {				\
	struct type *swap_first = SLIST_FIRST(head1);			\
	SLIST_FIRST(head1) = SLIST_FIRST(head2);			\
	SLIST_FIRST(head2) = swap_first;				\
} while (0)
#endif
#ifndef LIST_PREV
#define	LIST_PREV(elm, head, type, field)				\
	((elm)->field.le_prev == &LIST_FIRST((head)) ? NULL :		\
	    __containerof((elm)->field.le_prev, struct type, field.le_next))
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

	/* N.B. Don't put any data above this line or list pointers below
	 * without fixing edit_cpy(). */

	unsigned	 e_kind;	/* enum ed_typem | ED_FLAGS */
	unsigned short	 e_fmode;	/* new (SVN) mode (mkdir/addfile/prop) */

	char		*e_copyfrom;
	unsigned	 e_copyrev;

	char		*e_diff;
	size_t		 e_difflen;

	unsigned char	 e_preimage_md5[16];
	unsigned char	 e_postimage_md5[16];

	/* Only used by brancher: */
	git_oid		 e_old_sha1;
	git_oid		 e_new_sha1;
};

/* A logical SVN revision. */
struct branch_rev {
	TAILQ_ENTRY(branch_rev)		rv_list;

	unsigned			rv_rev;
	struct hashmap			rv_edits;
	TAILQ_HEAD(, br_edit)		rv_editorder;

	git_oid				rv_parent;

	/* Normally commits will only touch one branch. A common exception is
	 * r1, when the SVN user populates the tree structure. We'll do some
	 * verification and assume a revision only touches one branch for
	 * non-empty (in the git sense -- directory-only) commits. */
	bool				rv_only_empty_dirs;
	const char			*rv_branch;

	char				*rv_author;
	char				*rv_logmsg;
	git_time_t			rv_timestamp;

	/* This is real ugly, but SVN revs *can* touch multiple git branches
	 * (e.g., 'svn mv branch1/ branch2/'. */
	SLIST_HEAD(_afl, branch_rev)	rv_affil;
	SLIST_ENTRY(branch_rev)		rv_afflink;
	bool				rv_secondary;
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
extern git_repository *g_git_repo;
extern bool option_debugging;

void isvn_g_lock(void);
void isvn_g_unlock(void);
void isvn_fetcher_getrange(unsigned *revlo, unsigned *revhi, bool *done);
void isvn_mark_fetchdone(unsigned revlo, unsigned revhi);
void isvn_mark_commitdone(unsigned revlo, unsigned revhi);
void _isvn_commitdrain_add(unsigned rev, int);
void isvn_commitdone_dump(void);
/* Blocks until all revs up to 'rev' are fetched. */
void isvn_wait_fetch(unsigned rev);
bool isvn_all_fetched(void);
void isvn_assert_commit(const char *branch, unsigned rev);
bool isvn_has_commit(unsigned rev);

static inline void
isvn_commitdrain_inc(unsigned rev)
{
	_isvn_commitdrain_add(rev, 1);
}

static inline void
isvn_commitdrain_dec(unsigned rev)
{
	_isvn_commitdrain_add(rev, -1);
}

void isvn_editor_init(void);
void isvn_editor_inialize_dedit_obj(svn_delta_editor_t *de);
bool path_startswith(const char *haystack, const char *needle);
const char *strip_branch(const char *path, const char **branch_out);
void edit_mergeinto(struct br_edit *dst, struct br_edit *src);

void isvn_fetch_init(void);
void *isvn_fetch_worker(void *dummy_i);
struct isvn_client_ctx *get_svn_ctx(void);
void assert_noerr(svn_error_t *err, const char *fmt, ...);
void assert_status_noerr(apr_status_t status, const char *fmt, ...);
void branch_edit_free(struct br_edit *edit);
struct branch_rev *new_branch_rev(svn_revnum_t rev);
void branch_rev_free(struct branch_rev *br);
void branch_rev_mergeinto(struct branch_rev *dst, struct branch_rev *src);

void isvn_brancher_init(void);
struct svn_branch *svn_branch_get(struct hashmap *h, const char *name);
void svn_branch_hash_init(struct hashmap *hash);
void svn_branch_revs_enqueue_and_free(struct svn_branch *branch);
void svn_branch_append(struct svn_branch *sb, struct branch_rev *br);
void *isvn_bucket_worker(void *v);

void isvn_revmap_init(void);
void isvn_revmap_insert(unsigned revnum, const char *branch, const git_oid *sha1);
/* Scans from 'rev' downward looking for 'branch'.
 * The usual negative return => error (ENOENT).
 * Sets 'sha1_out' to the hash of the commit if found (no error). */
int isvn_revmap_lookup_branchlatest(const char *branch, unsigned rev,
	git_oid *sha1_out);
void isvn_dump_revmap(void);

/* XXX Transitioning */
#define die		isvn_die
#define die_errno	isvn_die_errno

void die(const char *fmt, ...) __attribute__((format(printf, 1, 2)))
	__attribute__((noreturn));
void die_errno(const char *fmt, ...) __attribute__((format(printf, 1, 2)))
	__attribute__((noreturn));

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

#ifndef __unused
#define __unused __attribute__((unused))
#endif

/*
 * TODO: Transitioning APIs.
 * OOM-safe APIs to implement.
 * strbuf_*() are nice; do them if needed.
 */
#define xasprintf(...)	do {			\
	int __rc_unused;			\
	__rc_unused = asprintf(__VA_ARGS__);	\
	(void) __rc_unused;			\
} while (false)
#define xstrdup(X) strdup(X)
#define xmalloc(X) malloc(X)
#define xcalloc(X, Y) calloc(X, Y)
#define xrealloc(X, Y) realloc(X, Y)

struct usage_option {
	char		flag;
	const char	*longname;
	const char	*usage;
	int		has_arg;
	void		*extra;
};

/* XXX Transition */
#define memhash		isvn_memhash
#define memintern	isvn_memintern
#define strhash		isvn_strhash
#define strintern	isvn_strintern

unsigned memhash(const void *, size_t);
const void *memintern(const void *, size_t);

static inline const char *
strintern(const char *str)
{

	return memintern(str, strlen(str));
}

static inline unsigned
strhash(const char *str)
{

	return memhash(str, strlen(str));
}

int create_leading_directories(const char *);
void isvn_complete_line(char *buf, size_t bufsz);

/* XXX Transition */
#define strip_suffix_mem	isvn_strip_suffix_mem
#define xstrndup		isvn_xstrndup
int strip_suffix_mem(const char *buf, size_t *len, const char *suffix);
char *xstrndup(const char *, size_t);

void md5_fromstr(unsigned char md5[16], const char *str);
void md5_tostr(char str[33], unsigned char md5[16]);

void isvn_git_compat_init(void);

#endif
