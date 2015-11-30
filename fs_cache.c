#include "cache.h"
#include "fs_cache.h"
#include "strbuf.h"
#include "hashmap.h"
#include "hash-io.h"
#include "lockfile.h"

#define FS_CACHE_SIGNATURE 0x4D4F4443	/* "MODC" */

struct trace_key trace_watchman = TRACE_KEY_INIT(WATCHMAN);

struct fs_cache the_fs_cache;

static int cached_umask = -1;

static int fe_entry_cmp(const struct fsc_entry *f1,
			const struct fsc_entry *f2,
			const char *name)
{
	if (f1->pathlen != f2->pathlen)
		return 1;
	name = name ? name : f2->path;
	return ignore_case ? strncasecmp(f1->path, name, f1->pathlen) :
		strncmp(f1->path, name, f1->pathlen);

}

unsigned char fe_dtype(struct fsc_entry *file)
{
	if (fe_is_reg(file)) {
		return DT_REG;
	}
	if (fe_is_dir(file)) {
		return DT_DIR;
	}
	if (fe_is_lnk(file)) {
		return DT_LNK;
	}
	return DT_UNKNOWN;
}

#define FS_CACHE_FORMAT_LB 1
#define FS_CACHE_FORMAT_UB 3

static int verify_hdr(struct fs_cache_header *hdr, unsigned long size)
{
	vmac_ctx_t c;
	unsigned char sha1[20];
	int hdr_version;
	unsigned char *key = (unsigned char *)"abcdefghijklmnop";

	if (hdr->hdr_signature != htonl(FS_CACHE_SIGNATURE)) {
		warning("bad fs_cache signature");
		return -1;
	}
	hdr_version = ntohl(hdr->hdr_version);
	/* Version 1 is deprecated; let's just pretend it's not there */
	if (hdr_version == 1)
		return -1;

	if (hdr_version < FS_CACHE_FORMAT_LB || FS_CACHE_FORMAT_UB < hdr_version) {
		warning("bad fs_cache version %d", hdr_version);
		return -1;
	}

	vmac_set_key(key, &c);
	vmac_update_unaligned(hdr, size - 20, &c);
	vmac_final(sha1, &c);
	if (hashcmp(sha1, (unsigned char *)hdr + size - 20)) {
		warning("bad fs_cache file vmac signature");
		return -1;
	}
	return 0;
}

static uid_t uid;
static gid_t gid;

static struct fsc_entry *create_from_disk(struct ondisk_fsc_entry *disk_fe, unsigned long *consumed, const char *prev)
{
	struct fsc_entry *fe;
	int pathlen = ntohl(disk_fe->pathlen);
	unsigned shared;

	fe = obstack_alloc(&the_fs_cache.obs, sizeof(*fe) + pathlen + 1);

	fe->in_index = 0;
	fe->st.st_size = ntohl(disk_fe->size);
	fe->st.st_mode = ntohl(disk_fe->mode);
	fe->flags = ntohl(disk_fe->flags);

	fe->st.st_ctime = ntohl(disk_fe->ctime);
	fe->st.st_mtime = ntohl(disk_fe->mtime);

	fe->st.st_ino = ntohl(disk_fe->ino);
	fe->st.st_dev = ntohl(disk_fe->dev);

	fe->st.st_uid = uid;
	fe->st.st_gid = gid;

	fe->parent = NULL;
	fe->first_child = NULL;
	fe->next_sibling = NULL;

	shared = (unsigned char)disk_fe->path[0];
	memcpy(fe->path, prev, shared);
	memcpy(fe->path + shared, disk_fe->path + 1,
	       pathlen + 1 - shared);
	fe->pathlen = pathlen;

	hashmap_entry_init(fe, ntohl(disk_fe->hash));
	*consumed = sizeof(*disk_fe) + pathlen + 2 - shared;
	return fe;
}

static int copy_fs_cache_entry_to_ondisk(
	struct ondisk_fsc_entry *ondisk,
	struct fsc_entry *fe, const char *prev)
{
	int i;
	ondisk->hash = htonl(fe->ent.hash);
	ondisk->pathlen = htonl(fe->pathlen);

	ondisk->size = htonl(fe->st.st_size);
	ondisk->mode = htonl(fe->st.st_mode);
	ondisk->flags = htonl(fe->flags & ~FE_NEW);

	ondisk->ctime = htonl(fe->st.st_ctime);
	ondisk->mtime = htonl(fe->st.st_mtime);

	ondisk->ino = htonl(fe->st.st_ino);
	ondisk->dev = htonl(fe->st.st_dev);

	for (i = 0; i < 255; ++i)
		if (!prev[i] || fe->path[i] != prev[i])
			break;
	ondisk->path[0] = i;
	memcpy(ondisk->path + 1, fe->path + i, fe->pathlen + 1 - i);

	return sizeof(*ondisk) + fe->pathlen + 2 - i;
}

static int fe_write_entry(struct fsc_entry *fe, int fd, struct hash_context *context, const char **prev)
{
	int result;
	static struct ondisk_fsc_entry *ondisk = NULL;
	static size_t max_size = sizeof(*ondisk) + 1 + PATH_MAX;
	size_t size;

	size = sizeof(*ondisk) + fe->pathlen + 2;
	if (size > max_size) {
		max_size = size;
		if (ondisk) {
			ondisk = xrealloc(ondisk, max_size);
			memset(ondisk, 0, max_size);
		}
	}


	if (!ondisk)
		ondisk = xcalloc(1, max_size);

	size = copy_fs_cache_entry_to_ondisk(ondisk, fe, *prev);
	*prev = fe->path;

	result = write_with_hash(context, fd, ondisk, size);

	return result ? -1 : 0;
}

static int fe_write_entry_recursive(struct fsc_entry *fe, int fd, struct hash_context *c, const char **prev)
{
	if (fe_write_entry(fe, fd, c, prev))
		return error("failed to write some file of fs_cache");
	fe = fe->first_child;

	while (fe) {
		if (fe_write_entry_recursive(fe, fd, c, prev))
			return 1;
		fe = fe->next_sibling;
	}
	return 0;
}

static char *xstrcpy(char *dest, const char *src)
{
	while ((*dest++ = *src++)) {
	}

	return dest;
}

int write_fs_cache(void)
{
	struct hash_context c;
	struct fs_cache_header *hdr;
	int hdr_size;
	struct stat st;
	int fd;
	const char *path;
	char *cur;
	const char *prev = "";
	int string_size;
	static struct lock_file lock;

	assert(fs_cache_is_valid());

	the_fs_cache.version = FS_CACHE_FORMAT_UB;

	trace_printf_key(&trace_watchman, "Started writing fs_cache.  last_update=%s\n", the_fs_cache.last_update);
	path = get_fs_cache_file();

	fd = hold_lock_file_for_update_retry_stale(&lock, path, 0);
	if (fd < 0) {
		trace_printf_key(&trace_watchman, "Failed to lock watchman cache\n");
		return 1;
	}

	string_size = strlen(the_fs_cache.last_update) +
		strlen(the_fs_cache.repo_path) +
		strlen(the_fs_cache.excludes_file) + 3;

	hdr_size = sizeof(*hdr) + string_size;
	hdr = xmalloc(hdr_size);
	hdr->hdr_signature = htonl(FS_CACHE_SIGNATURE);
	hdr->hdr_version = htonl(the_fs_cache.version);
	hdr->hdr_entries = htonl(the_fs_cache.nr);
	hdr->flags = htonl(the_fs_cache.flags);
	hashcpy(hdr->git_excludes_sha1, the_fs_cache.git_excludes_sha1);
	hashcpy(hdr->user_excludes_sha1, the_fs_cache.user_excludes_sha1);
	cur = xstrcpy(hdr->strings, the_fs_cache.last_update);
	cur = xstrcpy(cur, the_fs_cache.repo_path);
	cur = xstrcpy(cur, the_fs_cache.excludes_file);

	hash_context_init(&c, HASH_IO_VMAC);

	if (write_with_hash(&c, fd, hdr, hdr_size) < 0)
		die_errno("failed to write header of fs_cache");

	if (fe_write_entry_recursive(fs_cache_file_exists("", 0),
				     fd, &c, &prev))
		return error("Failed to write fs_cache entries");

	if (write_with_hash_flush(&c, fd) || fstat(fd, &st))
		return error("Failed to flush/fstat fs_cache file");

	commit_lock_file(&lock);
	hash_context_release(&c);
	free(hdr);

	trace_printf_key(&trace_watchman, "Wrote fs_cache. last_update=%s\n", the_fs_cache.last_update);
	return 0;
}

void *mmap_fs_cache(size_t *mmap_size)
{
	struct stat st;
	void *mmap;
	const char *path = get_fs_cache_file();
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return NULL;
		die_errno("fs_cache file open failed");
	}

	if (fstat(fd, &st))
		die_errno("cannot stat the open fs_cache");

	*mmap_size = xsize_t(st.st_size);
	if (*mmap_size < sizeof(struct fs_cache_header) + 20) {
		warning("fs_cache file smaller than expected");
		return NULL;
	}

	mmap = xmmap(NULL, *mmap_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (mmap == MAP_FAILED)
		die_errno("unable to map fs_cache file");
	close(fd);
	return mmap;
}

/* Loading the fs_cache can take some time, and we might want to thread
 * it with other loads; we need the last-update time and the repo path
 * to check whether this is a good idea, so this function will preload
 * it.  Note that the caller must free the returned strings.
 */
void fs_cache_preload_metadata(char **last_update, char **repo_path)
{
	size_t mmap_size;
	void *mmap;
	struct fs_cache_header *hdr;
	int version;

	mmap = mmap_fs_cache(&mmap_size);
	if (!mmap) {
		*last_update = *repo_path = NULL;
		return;
	}
	hdr = mmap;
	version = ntohl(hdr->hdr_version);
	/* Version 1 is deprecated; let's just pretend it's not there */
	if (version == 1)
		goto unmap;

	if (version < FS_CACHE_FORMAT_LB || FS_CACHE_FORMAT_UB < version) {
		warning("bad fs_cache version %d", version);
		goto unmap;
	}

	*last_update = xstrdup(hdr->strings);
	*repo_path = xstrdup(hdr->strings + strlen(*last_update) + 1);

unmap:
	munmap(mmap, mmap_size);
}

static void write_fs_cache_if_necessary(void)
{
	if (fs_cache_is_valid() && the_fs_cache.needs_write) {
		write_fs_cache();
		clear_fs_cache();
	}
}

static void fe_set_parent(struct fsc_entry *fe, struct fsc_entry *parent)
{
	fe->parent = parent;
	fe->next_sibling = fe->parent->first_child;
	fe->parent->first_child = fe;
}

int set_up_parent(struct fsc_entry *fe)
{
	char *last_slash;
	int parent_len;
	struct fsc_entry *parent;
	if (fe->pathlen == 0)
		return 0;

	last_slash = strrchr(fe->path, '/');

	if (last_slash) {
		parent_len = last_slash - fe->path;
	} else {
		parent_len = 0;
	}

	parent = fs_cache_file_exists(fe->path, parent_len);
	if (!parent) {
		/* Bug in watchman itself; notify caller of failure */
		warning ("Potential watchman bug: cannot find parent for %s",
			 fe->path);
		return 1;
	}
	fe_set_parent(fe, parent);
	return 0;
}

static char *read_string(char **out, char *in)
{
	int len = strlen(in);
	*out = xstrdup(in);
	return in + len + 1;
}

int fs_cache_is_valid(void)
{
	return the_fs_cache.last_update != NULL && the_fs_cache.fully_loaded;
}

/*
 * Load the modified file cache from disk.  Returns 1 if it resulted in a valid
 * fs_cache, or 0 if it was unable due to missing/corrupt.
 */
int read_fs_cache(void)
{
	struct fs_cache_header *hdr;
	int i;
	unsigned int nr;
	void *mmap;
	void *mmap_cur;
	size_t mmap_size;
	struct fsc_entry *parent_stack[PATH_MAX];
	int parent_top = -1;
	const char *prev = "";

	if (fs_cache_is_valid()) {
		return 1;
	}

	mmap = mmap_fs_cache(&mmap_size);
	if (!mmap) {
		return 0;
	}

	hdr = mmap;
	if (verify_hdr(hdr, mmap_size) < 0)
		goto unmap;

	clear_fs_cache();
	nr = ntohl(hdr->hdr_entries);
	obstack_begin(&the_fs_cache.obs, nr * (sizeof(struct fsc_entry) + FSC_ENTRY_AVG_PATH_LENGTH));
	the_fs_cache.flags = ntohl(hdr->flags);
	the_fs_cache.version = ntohl(hdr->hdr_version);
	hashmap_init(&the_fs_cache.paths, (hashmap_cmp_fn) fe_entry_cmp, nr);
	the_fs_cache.nr = 0;
	hashcpy(the_fs_cache.git_excludes_sha1, hdr->git_excludes_sha1);
	hashcpy(the_fs_cache.user_excludes_sha1, hdr->user_excludes_sha1);

	mmap_cur = hdr->strings;
	mmap_cur = read_string(&the_fs_cache.last_update, mmap_cur);
	mmap_cur = read_string(&the_fs_cache.repo_path, mmap_cur);
	mmap_cur = read_string(&the_fs_cache.excludes_file, mmap_cur);

	uid = getuid();
	gid = getgid();

	for (i = 0; i < nr; i++) {
		struct fsc_entry *fe;
		unsigned long consumed;

		fe = create_from_disk(mmap_cur, &consumed, prev);

		prev = fe->path;
		/*
		 * We eliminate deleted cache entries on read because
		 * otherwise we have to count them in advance to fill
		 * in nr, and that would be expensive.
		 */
		if (!fe_deleted(fe)) {
			fs_cache_insert(fe);
			if (parent_top == -1) {
				parent_top = 0;
				parent_stack[0] = fe;
			} else {
				char *p = parent_stack[parent_top]->path;
				char *c = fe->path;
				parent_top = 1;
				for (; *p && *c; ++p, ++c) {
					if (*p != *c)
						break;
					if (*p == '/')
						parent_top ++;
				}
				if (*p == 0 && *c == '/')
					parent_top ++;
				parent_stack[parent_top] = fe;
				fe_set_parent(fe, parent_stack[parent_top - 1]);
			}
		}
		mmap_cur = (char *)mmap_cur + consumed;
	}

	munmap(mmap, mmap_size);

	the_fs_cache.fully_loaded = 1;

	atexit(write_fs_cache_if_necessary);
	return fs_cache_is_valid();

unmap:
	munmap(mmap, mmap_size);
	return 0;
}

void clear_fs_cache(void)
{
	obstack_free(&the_fs_cache.obs, NULL);
	free(the_fs_cache.last_update);
	free(the_fs_cache.repo_path);
	free(the_fs_cache.excludes_file);

	memset(&the_fs_cache, 0, sizeof(the_fs_cache));

	the_fs_cache.version = FS_CACHE_FORMAT_UB;
	the_fs_cache.needs_write = 0;
	the_fs_cache.fully_loaded = 0;
	hashmap_init(&the_fs_cache.paths, (hashmap_cmp_fn) fe_entry_cmp, 1);
	atexit(write_fs_cache_if_necessary);
}

struct fsc_entry *fs_cache_file_exists(const char *name, int namelen)
{
	return fs_cache_file_exists_prehash(name, namelen, memihash(name, namelen));
}

struct fsc_entry *fs_cache_file_exists_prehash(const char *path, int pathlen, unsigned int hash)
{
	struct fsc_entry key;

	hashmap_entry_init(&key, hash);
	key.pathlen = pathlen;
	return hashmap_get(&the_fs_cache.paths, &key, path);
}

struct fsc_entry *make_fs_cache_entry(const char *path)
{
	return make_fs_cache_entry_len(path, strlen(path));
}

struct fsc_entry *make_fs_cache_entry_len(const char *path, int len)
{
	struct fsc_entry *fe = xcalloc(1, sizeof(*fe) + len + 1);
	fe_set_new(fe);
	fe->in_index = 0;
	memcpy(fe->path, path, len);
	fe->pathlen = len;
	hashmap_entry_init(fe, memihash(fe->path, fe->pathlen));
	return fe;
}

void fs_cache_insert(struct fsc_entry *fe)
{
	hashmap_put(&the_fs_cache.paths, fe);
	the_fs_cache.nr++;
}

static void fs_cache_remove_recursive(struct fsc_entry *fe)
{
	struct fsc_entry *cur, *next;

	assert(fs_cache_is_valid());

	for (cur = fe->first_child; cur; cur = next) {
		fs_cache_remove_recursive(cur);
		next = cur->next_sibling;
		cur->next_sibling = NULL;
		cur->parent = NULL;
		cur->first_child = NULL;
	}

	hashmap_remove(&the_fs_cache.paths, fe, NULL);
	the_fs_cache.nr--;
}

static void fe_remove_from_parent(struct fsc_entry *fe)
{
	struct fsc_entry *prev, *cur;
	if (fe->parent) {
		prev = NULL;
		for (cur = fe->parent->first_child; cur; cur = cur->next_sibling) {
			if (cur == fe) {
				if (prev)
					prev->next_sibling = fe->next_sibling;
				else
					fe->parent->first_child = fe->next_sibling;
				break;
			}
			prev = cur;
		}
	}
}

void fe_delete_children(struct fsc_entry *fe)
{
	for (fe = fe->first_child; fe; fe = fe->next_sibling) {
		fe_set_deleted(fe);
	}
}

void fe_clear_children(struct fsc_entry *fe)
{
	for (fe = fe->first_child; fe; fe = fe->next_sibling) {
		fs_cache_remove(fe);
	}
}

void fe_set_deleted(struct fsc_entry *fe)
{
	fe->flags |= FE_DELETED;
	fe_delete_children(fe);
}

void fs_cache_remove(struct fsc_entry *fe)
{
	fs_cache_remove_recursive(fe);
	fe_remove_from_parent(fe);
}

int is_in_dot_git(const char *name)
{
	char *evil = ".git";
	char *cur = evil;
	while (*name) {
		if (*name == *cur++) {
			name++;
			if (*cur == 0) {
				if (*name == 0 || *name == '/') {
					return 1;
				}
			}
		} else {
			if (*name == '/') {
				cur = evil;
			} else {
				cur = "";
			}
			name++;
		}
	}
	return 0;
}

static int is_path_prefix(const char *putative_parent, const char *fname)
{
	const char* c;
	for (c = putative_parent; *c && *fname; ++c, ++fname) {
		if (*c != *fname) {
			return 0;
		}
	}
	return *c == 0 && (*fname == 0 || *fname == '/');
}

int open_using_fs_cache(const char *fname, int flags)
{
	if (fs_cache_is_valid() &&
	    fname[0] != '/' &&
	    !is_path_prefix(get_git_dir(), fname)) {
		struct fsc_entry *fe = fs_cache_file_exists(fname, strlen(fname));
		if (!fe || fe_deleted(fe)) {
			errno = ENOENT;
			return -1;
		}
	}
	return open(fname, flags);
}

static const int topological_rank[256] = {
	0, /* slash moved here */ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	1 /* slash is special */,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
	79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106,
	107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120,
	121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
	135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162,
	163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
	177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218,
	219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
	233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
	247, 248, 249, 250, 251, 252, 253, 254, 255
};

/*
 * Compare fsc_entry structs topologically -- that is, so that parent
 * directories come before their children.
 */
int cmp_fsc_entry(const void *a, const void *b)
{
	struct fsc_entry* const * sa = a;
	struct fsc_entry* const * sb = b;
	const unsigned char* pa = (unsigned char *)(*sa)->path;
	const unsigned char* pb = (unsigned char *)(*sb)->path;

	if (ignore_case)
		return icmp_topo(pa, pb);
	else
		return cmp_topo(pa, pb);
}

int cmp_topo(const unsigned char *pa, const unsigned char *pb) {
	while (*pa && *pb) {
		int ca = topological_rank[*pa++];
		int cb = topological_rank[*pb++];
		int diff = ca - cb;
		if (diff)
			return diff;
	}
	return topological_rank[*pa] - topological_rank[*pb];
}

/* Case-insensitive version of the above table */
static const int itopological_rank[256] = {
	0, /* slash moved here */ 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	1 /* slash is special */,
	48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
	63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
	79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
	95, 96, /* The lowercase letters are mapped to uppercase */
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
	79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 
	123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
	135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148,
	149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162,
	163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176,
	177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190,
	191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204,
	205, 206, 207, 208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218,
	219, 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232,
	233, 234, 235, 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246,
	247, 248, 249, 250, 251, 252, 253, 254, 255
};

int icmp_topo(const unsigned char *pa, const unsigned char *pb) {
	while (*pa && *pb) {
		int ca = itopological_rank[*pa++];
		int cb = itopological_rank[*pb++];
		int diff = ca - cb;
		if (diff)
			return diff;
	}
	return itopological_rank[*pa] - itopological_rank[*pb];
}

static int get_umask(void)
{
	if (cached_umask != -1)
		return cached_umask;
	cached_umask = umask(0777);
	umask(cached_umask);
	return cached_umask;
}

static void copy_ce_stat_to_fe(const struct cache_entry *ce, struct fsc_entry *fe)
{
	int on_disk_mask;
	fe_clear_deleted(fe);
	fe->st.st_size = ce->ce_stat_data.sd_size;
#ifdef __linux__
	/* On Linux, symlinks are always 0777 */
	if (S_ISLNK(ce->ce_mode)) {
		fe->st.st_mode = S_IFLNK | 0777;
	} else {
		on_disk_mask = 0666;
		fe->st.st_mode = ce->ce_mode | (on_disk_mask & ~get_umask());
	}
#else
	/*
	 * The ce_mode is git's internal mode -- not the on-disk mode.
	 * The on-disk mode will be 666 & ~umask for regular files,
	 * 777 & ~umask for symlinks, so we:
	 */
	on_disk_mask = S_ISLNK(ce->ce_mode) ? 0777 : 0666;
	fe->st.st_mode = ce->ce_mode | (on_disk_mask & ~get_umask());
#endif
	fe->st.st_ino = ce->ce_stat_data.sd_ino;
	fe->st.st_dev = ce->ce_stat_data.sd_dev;
	fe->st.st_uid = ce->ce_stat_data.sd_uid;
	fe->st.st_gid = ce->ce_stat_data.sd_gid;
	fe->st.st_mtime = ce->ce_stat_data.sd_mtime.sec;
	fe->st.st_ctime = ce->ce_stat_data.sd_ctime.sec;
}

static struct fsc_entry *ce_to_fs_cache_entry(const struct cache_entry *ce)
{
	struct fsc_entry *fe = make_fs_cache_entry_len(ce->name, ce_namelen(ce));
	assert(fe);

	copy_ce_stat_to_fe(ce, fe);
	fe->in_index = 1;
	if ((ce->ce_mode & S_IFMT) == S_IFGITLINK)
		fe->st.st_mode = S_IFDIR;

	return fe;
}

/**
 * Given a path, insert all leading directories in the fs_cache.
 * This assumes that the final file in the path is not a directory
 */
void insert_leading_dirs_in_fscache(const char *path)
{
	int i;
	char *subdir = xmalloc(strlen(path)+1);
	assert(subdir);
	assert(path);

	for (i=0; path[i] != '\0'; i++) {
		if (i>0 && path[i] == '/') {
			subdir[i] = '\0';
			insert_path_in_fscache(subdir);
		}
		subdir[i] = path[i];
	}

	free(subdir);
}

/** insert a path in fs_cache */
void insert_path_in_fscache(const char *path)
{
	struct fsc_entry *fe = fs_cache_file_exists(path, strlen(path));

	assert(fs_cache_is_valid());

	if (!fe) {
		trace_printf_key(&trace_watchman, "Adding %s to fs_cache\n", path);
		fe = make_fs_cache_entry(path);
		fs_cache_insert(fe);
		if (set_up_parent(fe)) {
			/* in the catastrophic case of being unable to find the parent, just remove the entry */
			fs_cache_remove(fe);
		}
	}

	if (0 != lstat(path, &fe->st)) {
		die_errno("Cannot stat a directory we just created: '%s'", path);
	}
	fe_clear_deleted(fe);
	fe->in_index = 1;
}

void insert_ce_in_fs_cache(struct cache_entry *ce)
{
	struct fsc_entry *fe = fs_cache_file_exists(ce->name, ce_namelen(ce));
	if (fe) {
		copy_ce_stat_to_fe(ce, fe);
	}
	else {
		fe = ce_to_fs_cache_entry(ce);
		trace_printf_key(&trace_watchman, "Adding ce for %s to fs_cache\n", ce->name);
		fs_cache_insert(fe);
		if (set_up_parent(fe)) {
			/* this will warn but hopefully watchman will backstop us */
			fs_cache_remove(fe);
		}
	}
}
