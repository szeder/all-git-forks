#ifndef LMDB_UTILS_H
#define LMDB_UTILS_H

#include <lmdb.h>

void mdb_put_or_die(MDB_val *key, MDB_val *val, int mode);
int mdb_get_or_die(MDB_val *key, MDB_val *val);
void mdb_cursor_open_or_die(MDB_cursor **cursor);
int mdb_cursor_get_or_die(MDB_cursor *cursor, MDB_val *key, MDB_val *val,
			  int mode);
int lmdb_init(const char *db, int flags, int create);
void lmdb_txn_commit(void);
void lmdb_txn_abort(void);

#endif
