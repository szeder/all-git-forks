#include "cache.h"
#include "lmdb-utils.h"

static MDB_txn *lmdb_txn;
static MDB_dbi lmdb_dbi;
static MDB_env *lmdb_env;

void mdb_put_or_die(MDB_val *key, MDB_val *val, int mode)
{
	int ret = mdb_put(lmdb_txn, lmdb_dbi, key, val, mode);

	if (ret)
		die("mdb_put failed: %s", mdb_strerror(ret));
}

int mdb_get_or_die(MDB_val *key, MDB_val *val)
{
	int ret;

	ret = mdb_get(lmdb_txn, lmdb_dbi, key, val);
	if (ret) {
		if (ret != MDB_NOTFOUND)
			die("mdb_get failed: %s", mdb_strerror(ret));
		return ret;
	}
	return 0;
}

void mdb_cursor_open_or_die(MDB_cursor **cursor)
{
	int ret;

	ret = mdb_cursor_open(lmdb_txn, lmdb_dbi, cursor);
	if (ret)
		die("mdb_cursor_open failed: %s", mdb_strerror(ret));
}

int mdb_cursor_get_or_die(MDB_cursor *cursor, MDB_val *key, MDB_val *val, int mode)
{
	int ret;

	ret = mdb_cursor_get(cursor, key, val, mode);
	if (ret && ret != MDB_NOTFOUND)
		die("mdb_cursor_get failed: %s", mdb_strerror(ret));

	return ret;
}

static void init_env(MDB_env **env, const char *path)
{
	int ret;
	if (*env)
		return;

	if ((ret = mdb_env_create(env)) != MDB_SUCCESS)
		die("mdb_env_create failed: %s", mdb_strerror(ret));
	if ((ret = mdb_env_set_maxreaders(*env, 1000)) != MDB_SUCCESS)
		die("BUG: mdb_env_set_maxreaders failed: %s", mdb_strerror(ret));
	if ((ret = mdb_env_set_mapsize(*env, (1<<30))) != MDB_SUCCESS)
		die("BUG: mdb_set_mapsize failed: %s", mdb_strerror(ret));

	if ((ret = mdb_env_open(*env, path, 0 , 0664)) != MDB_SUCCESS)
		die("BUG: mdb_env_open (%s) failed: %s", path, mdb_strerror(ret));
}

int lmdb_init(const char *db, int flags, int create)
{
	const char *path;
	int ret;

	path = git_path("%s", db);

	if (!create) {
		struct stat st;
		if (stat(path, &st)) {
			if (errno == ENOENT)
				return -1;
			die("unable to stat %s", path);
		}
	}

	mkdir(path, 0775);

	init_env(&lmdb_env, path);
	if ((ret = mdb_txn_begin(lmdb_env, NULL, flags, &lmdb_txn)) != MDB_SUCCESS) {
		die("mdb_txn_begin failed: %s", mdb_strerror(ret));

	}
	if ((ret = mdb_dbi_open(lmdb_txn, NULL, 0, &lmdb_dbi)) != MDB_SUCCESS) {
		die("mdb_txn_open failed: %s", mdb_strerror(ret));
	}

	return 0;
}

void lmdb_txn_commit(void)
{
	mdb_txn_commit(lmdb_txn);
}

void lmdb_txn_abort(void)
{
	mdb_txn_abort(lmdb_txn);
}
