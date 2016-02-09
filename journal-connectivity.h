#ifndef JOURNAL_CONNECTIVITY_H
#define JOURNAL_CONNECTIVITY_H

struct packed_git;

enum pack_check_result {
	PACK_INVALID = -1,
	PACK_ADDED = 0,
	PACK_PRESENT = 1,
};

enum jcdb_transaction_flags {
	JCDB_CREATE = 1,
};

enum pack_check_result jcdb_check_and_record_pack(struct packed_git *pack);
void jcdb_record_update_ref(const unsigned char *old, const unsigned char *new);
int jcdb_pack_is_journaled(const unsigned char *sha);

struct jcdb_transaction {
	unsigned open:1;
};

int jcdb_transaction_begin(struct jcdb_transaction *transaction,
			   enum jcdb_transaction_flags flags);
int jcdb_add_pack(struct jcdb_transaction *transaction, const unsigned char *sha1);
void jcdb_transaction_commit(struct jcdb_transaction *transaction);
void jcdb_transaction_abort(struct jcdb_transaction *transaction);

void jcdb_packlog_dump(void);

void jcdb_backfill(void);

#endif
