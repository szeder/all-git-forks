/*
 * Declarations of reference-related utilities that are available for
 * use by any reference backend. This header file is *not* part of the
 * reference API and is meant *only* for the use of reference backend
 * implementations!
 */

#ifndef REFS_BE_COMMON_H
#define REFS_BE_COMMON_H

#include "lockfile.h"

/*
 * Flag passed to lock_ref_sha1_basic() telling it to tolerate broken
 * refs (i.e., because the reference is about to be deleted anyway).
 */
#define REF_DELETING	0x02

/*
 * Used as a flag in ref_update::flags when a loose ref is being
 * pruned.
 */
#define REF_ISPRUNING	0x04

/*
 * Used as a flag in ref_update::flags when the reference should be
 * updated to new_sha1.
 */
#define REF_HAVE_NEW	0x08

/*
 * Used as a flag in ref_update::flags when old_sha1 should be
 * checked.
 */
#define REF_HAVE_OLD	0x10

/*
 * Used as a flag in ref_update::flags when the lockfile needs to be
 * committed.
 */
#define REF_NEEDS_COMMIT 0x20

/*
 * 0x40 is REF_FORCE_CREATE_REFLOG, so skip it if you're adding a
 * value to ref_update::flags
 */

/**
 * Information needed for a single ref update. Set new_sha1 to the new
 * value or to null_sha1 to delete the ref. To check the old value
 * while the ref is locked, set (flags & REF_HAVE_OLD) and set
 * old_sha1 to the old value, or to null_sha1 to ensure the ref does
 * not exist before update.
 */
struct ref_update {
	/*
	 * If (flags & REF_HAVE_NEW), set the reference to this value:
	 */
	unsigned char new_sha1[20];
	/*
	 * If (flags & REF_HAVE_OLD), check that the reference
	 * previously had this value:
	 */
	unsigned char old_sha1[20];
	/*
	 * One or more of REF_HAVE_NEW, REF_HAVE_OLD, REF_NODEREF,
	 * REF_DELETING, and REF_ISPRUNING:
	 */
	unsigned int flags;
	struct ref_lock *lock;
	int type;
	char *msg;
	const char refname[FLEX_ARRAY];
};

/*
 * Transaction states.
 * OPEN:   The transaction is in a valid state and can accept new updates.
 *         An OPEN transaction can be committed.
 * CLOSED: A closed transaction is no longer active and no other operations
 *         than free can be used on it in this state.
 *         A transaction can either become closed by successfully committing
 *         an active transaction or if there is a failure while building
 *         the transaction thus rendering it failed/inactive.
 */
enum ref_transaction_state {
	REF_TRANSACTION_OPEN   = 0,
	REF_TRANSACTION_CLOSED = 1
};

/*
 * Data structure for holding a reference transaction, which can
 * consist of checks and updates to multiple references, carried out
 * as atomically as possible.  This structure is opaque to callers.
 */
struct ref_transaction {
	struct ref_update **updates;
	size_t alloc;
	size_t nr;
	enum ref_transaction_state state;
};

#endif /* REFS_BE_COMMON_H */
