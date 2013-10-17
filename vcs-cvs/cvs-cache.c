#include "vcs-cvs/cvs-cache.h"
#include "vcs-cvs/trace-utils.h"
#include "sigchain.h"

static DB *db_cache = NULL;
static DB *db_cache_branch = NULL;
static char *db_cache_branch_path = NULL;

static void db_cache_release(DB *db)
{
	if (db)
		db->close(db, 0);
}

static void db_cache_release_default()
{
	db_cache_release(db_cache);
	tracef("db_cache released\n");
	if (db_cache_branch) {
		db_cache_release(db_cache_branch);
		db_cache_branch = NULL;
		unlink(db_cache_branch_path);
		tracef("db_cache %s removed\n", db_cache_branch_path);
		free(db_cache_branch_path);
	}
}

static void db_cache_release_default_on_signal(int signo)
{
	db_cache_release_default();
	sigchain_pop(signo);
	raise(signo);
}

static DB *db_cache_init(const char *name, int *exists)
{
	DB *db;
	const char *db_dir;
	struct strbuf db_path = STRBUF_INIT;

	db_dir = getenv("GIT_CACHE_CVS_DIR");
	if (!db_dir)
		return NULL;

	if (db_create(&db, NULL, 0))
		die("cannot create db_cache descriptor");

	strbuf_addf(&db_path, "%s/%s", db_dir, name);

	if (exists) {
		*exists = 0;
		if (!access(db_path.buf, R_OK | W_OK))
			*exists = 1;
	}

	//db->set_bt_compress(db, NULL, NULL);

	if (db->open(db, NULL, db_path.buf, NULL, DB_BTREE, DB_CREATE, 0664) != 0)
		die("cannot open/create db_cache at %s", db_path.buf);

	strbuf_release(&db_path);
	return db;
}

void db_cache_init_default()
{
	if (db_cache)
		return;

	db_cache = db_cache_init("cvscache.db", NULL);
	atexit(db_cache_release_default);
	sigchain_push_common(db_cache_release_default_on_signal);
}

static DBT *dbt_set(DBT *dbt, void *buf, size_t size)
{
	memset(dbt, 0, sizeof(*dbt));
	dbt->data = buf;
	dbt->size = size;

	return dbt;
}

/*
 * TODO:
 * path should be module + file path, this will help with subprojects import
 */
void db_cache_add(DB *db, const char *path, const char *revision, int isexec, struct strbuf *file)
{
	struct strbuf key_sb = STRBUF_INIT;
	int rc;
	DBT key;
	DBT value;

	if (!db) {
		if (!db_cache)
			return;
		db = db_cache;
	}

	/*
	 * last byte of struct strbuf is used to store isexec bit.
	 */
	file->buf[file->len] = isexec;

	strbuf_addf(&key_sb, "%s:%s", revision, path);
	dbt_set(&key, key_sb.buf, key_sb.len);
	dbt_set(&value, file->buf, file->len+1); // +1 for isexec bit
	rc = db->put(db, NULL, &key, &value, DB_NOOVERWRITE);
	if (rc && rc != DB_KEYEXIST)
		error("db_cache put failed with rc: %d", rc);

	file->buf[file->len] = 0;
	strbuf_release(&key_sb);
}

int db_cache_get(DB *db, const char *path, const char *revision, int *isexec, struct strbuf *file)
{
	struct strbuf key_sb = STRBUF_INIT;
	int rc;
	DBT key;
	DBT value;

	if (!db) {
		if (!db_cache)
			return DB_NOTFOUND;
		db = db_cache;
	}

	strbuf_addf(&key_sb, "%s:%s", revision, path);
	dbt_set(&key, key_sb.buf, key_sb.len);
	memset(&value, 0, sizeof(value));
	rc = db->get(db, NULL, &key, &value, 0);
	if (rc && rc != DB_NOTFOUND)
		error("db_cache get failed with rc: %d", rc);

	if (!rc) {
		strbuf_reset(file);
		strbuf_add(file, value.data, value.size);
		/*
		 * last byte of value is used to store isexec bit.
		 */
		file->len--;
		*isexec = file->buf[file->len];
		file->buf[file->len] = 0;
	}

	strbuf_release(&key_sb);
	return rc;
}

DB *db_cache_init_branch(const char *branch, time_t date, int *exists)
{
	DB *db;
	struct strbuf db_name = STRBUF_INIT;

	strbuf_addf(&db_name, "cvscache.%s.%ld.db", branch, date);
	db = db_cache_init(db_name.buf, exists);
	if (db) {
		db_cache_branch = db;
		db_cache_branch_path = strbuf_detach(&db_name, NULL);
	}
	else {
		strbuf_release(&db_name);
	}
	return db;
}

void db_cache_release_branch(DB *db)
{
	db_cache_release(db);
	db_cache_branch = NULL;
	if (db_cache_branch_path) {
		free(db_cache_branch_path);
		db_cache_branch_path = NULL;
	}
}

static unsigned int hash_buf(const char *buf, size_t size)
{
	unsigned int hash = 0x12375903;

	while (size) {
		unsigned char c = *buf++;
		hash = hash*101 + c;
		size--;
	}
	return hash;
}

int db_cache_for_each(DB *db, handle_file_fn_t cb, void *data)
{
	DBC *cur;
	DBT key;
	DBT value;
	int rc;
	void *p;
	struct cvsfile file = CVSFILE_INIT;

	if (db->cursor(db, NULL, &cur, 0))
		return -1;

	memset(&key, 0, sizeof(key));
	memset(&value, 0, sizeof(value));
	while (!(rc = cur->get(cur, &key, &value, DB_NEXT))) {
		strbuf_reset(&file.path);
		strbuf_reset(&file.revision);
		file.isdead = 0;
		file.isbin = 0;
		file.ismem = 1;
		strbuf_reset(&file.file);

		p = memchr(key.data, ':', key.size);
		if (!p)
			die("invalid db_cache key format");

		strbuf_add(&file.revision, key.data, p - key.data);
		p++;
		strbuf_add(&file.path, p, key.size - (p - key.data));
		strbuf_add(&file.file, value.data, value.size);
		/*
		 * last byte of value is used to store isexec bit.
		 */
		file.file.len--;
		file.isexec = file.file.buf[file.file.len];
		file.file.buf[file.file.len] = 0;

		tracef("db_cache foreach file: %s rev: %s size: %zu isexec: %u hash: %u\n",
			file.path.buf, file.revision.buf, file.file.len, file.isexec, hash_buf(file.file.buf, file.file.len));
		cb(&file, data);
	}

	cvsfile_release(&file);
	cur->close(cur);
	if (rc == DB_NOTFOUND)
		return 0;
	return -1;
}
