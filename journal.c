#include "git-compat-util.h"
#include "cache.h"
#include "revision.h"
#include "refs.h"
#include "journal.h"
#include "journal-common.h"
#include "strbuf.h"
#include "lockfile.h"
#include "remote.h"
#include "safe-append.h"

struct journal {
	uint32_t serial;
	struct stat sb;
	int fd;
};

const char *journal_dir(void)
{
	return mkpath("%s/journals", get_object_directory());
}

const char *journal_remote_dir(const struct remote * const upstream)
{
	return (is_bare_repository() || upstream->mirror)
		? journal_dir()
		: mkpath("%s/%s", journal_dir(), upstream->name);
}

static const char *journal_extents_file(void)
{
	return mkpath("%s/extents.bin", journal_dir());
}

static const char *journal_integrity_file(void)
{
	return mkpath("%s/integrity.bin", journal_dir());
}

static const char *metadata_path(void)
{
	return mkpath("%s/metadata.bin", journal_dir());
}

static const char *journal_path(uint32_t serial, char *to, size_t max)
{
	return mkpath("%s/%"PRIx32".bin", journal_dir(), serial);
}

static const char *journal_state_file(void)
{
	return mkpath("%s/state.bin", journal_dir());
}

static struct index_entry_t *index_entry_new(struct journal_ctx *c)
{
	struct index_entry_t *r;

	ALLOC_GROW(c->index_entries.items,
			c->index_entries.nr + 1,
			c->index_entries.alloc);
	r = &c->index_entries.items[c->index_entries.nr++];
	memset(r, 0, sizeof(*r));
	return r;
}

void journal_metadata_store(const char *path, struct journal_metadata *m)
{
	struct journal_metadata m2;
	static struct lock_file lock;
	int fd;

	m->magic[0] = 'T';
	m->magic[1] = 'G';

	if (memcpy(&m2, m, sizeof(m2)) != &m2)
		die("memcpy");

	fd = hold_lock_file_for_update(&lock, path, LOCK_DIE_ON_ERROR);

	if (fd < 0)
		die_errno("unable to open metadata for storage");

	m2.journal_serial = htonl(m2.journal_serial);

	if (write_in_full(fd, &m2, sizeof(m2)) != sizeof(m2))
		die_errno("unable to write metadata");

	commit_lock_file(&lock);
}

static void metadata_store(struct journal_metadata *m)
{
	journal_metadata_store(metadata_path(), m);
}

static void metadata_load(struct journal_metadata *m)
{
	int fd = open(metadata_path(), O_RDONLY);

	if (fd < 0)
		die_errno("unable to open metadata for retrieval");

	if (read_in_full(fd, m, sizeof(*m)) != sizeof(*m))
		die_errno("unable to read metadata");

	m->journal_serial = ntohl(m->journal_serial);

	if (m->magic[0] != 'T' || m->magic[1] != 'G')
		die_errno("bad metadata header");

	close(fd);
}

static struct journal *journal_open(uint32_t serial)
{
	struct journal *j = xcalloc(1, sizeof(struct journal));
	const char *path;

	j->serial = serial;

	path = journal_path(serial, (char *)&path, PATH_MAX);
	j->fd = open_safeappend_file(path, O_CREAT | O_RDWR, 0600);
	if (j->fd < 0)
		die_errno("could not open journal '%s'", path);

	return j;
}

static void journal_close(struct journal_ctx *c, struct journal *j)
{
	const char *path;

	path = journal_path(j->serial, (char *)&path, PATH_MAX);
	if (j->fd != 0 && commit_safeappend_file(path, j->fd) != 0)
		die_errno("error closing journal");

	free(j);
}

static struct journal *journal_with_capacity(struct journal_ctx *c, size_t required_bytes)
{
	const size_t overhead = 100; /* pessimistic */
	uint32_t serial = c->meta.journal_serial;
	struct journal *j;

	if (required_bytes + overhead >= c->size_limit)
		die("object (%zu bytes) exceeds journal size limit (%lu bytes)",
		    required_bytes, c->size_limit);

	while (1) {
		off_t o;

		j = journal_open(serial);
		o = lseek(j->fd, 0, SEEK_END);
		if ((o + required_bytes) < c->size_limit)
			break;
		fchmod(j->fd, 0400);
		journal_close(c, j);
		++serial;
	}

	if (c->meta.journal_serial != serial) {
		c->meta.journal_serial = serial;
		metadata_store(&c->meta);
		trace_printf("Triggered new journal (%"PRIx32"), updated metadata\n",
			     c->meta.journal_serial);
	}

	return j;
}

static void journal_setup(struct journal_ctx *c)
{
	struct stat s;
	/* stat 'objects' so we make our dirs have the same mode */
	char *obj_dir = get_object_directory();

	if (stat(obj_dir, &s) != 0)
		die_errno("unable to stat objects directory '%s'", obj_dir);

	/* make the journals directory if necessary */
	if (mkdir(journal_dir(), s.st_mode) < 0) {
		if (errno != EEXIST)
			die_errno("unable to create journal directory");
	}

	/* initialize metadata if necessary */
	if (stat(metadata_path(), &s) != 0 && errno == ENOENT) {
		memset(&c->meta, 0, sizeof(c->meta));
		c->meta.journal_serial = 0;
		metadata_store(&c->meta);
		printf("Journal initialized\n");
	} else if (errno != EEXIST) {
		die_errno("unable to stat metadata");
	}
}

static int lock_for_safeappend_with_delayed_retry(struct safeappend_lock *lock,
						  const char *path)
{
	size_t tries;

	for (tries = 100; tries > 0; tries--) {
		int fd;
		if (tries < 100 && tries % 25 == 0) {
			warning("waiting to lock %s", path);
		}
		fd = lock_safeappend_file(lock, path, 0);
		if (fd > 0)
			return fd;
		usleep(100000);
	}
	die("failed to obtain lock on %s", path);
}

static int integrity_fd(struct journal_ctx *c)
{
	return c->integrity_lock->fd;
}

static int extents_fd(struct journal_ctx *c)
{
	return c->extents_lock->fd;
}

static size_t integrity_record_count(struct journal_ctx *c)
{
	struct stat sb;

	if (fstat(integrity_fd(c), &sb) != 0) {
		if (errno == ENOENT)
			return 0;
		else
			die("fstat failed");
	}
	return sb.st_size / sizeof(struct journal_integrity_rec);
}

static void integrity_last_record(struct journal_ctx *c, struct journal_integrity_rec *to)
{
	size_t count = integrity_record_count(c);
	off_t orig_pos, pos;
	ssize_t r;

	memset(to, 0, sizeof(*to));
	if (count <= 0)
		return;

	orig_pos = lseek(integrity_fd(c), 0, SEEK_END);
	pos = orig_pos - sizeof(*to);
	if (lseek(integrity_fd(c), pos, SEEK_SET) != pos)
		die_errno("seek to record position failed (integrity file)");
	errno = 0;
	r = read_in_full(integrity_fd(c), to, sizeof(*to));
	if (r != sizeof(*to))
		die_errno("read failed (integrity file) (wanted %zu, got %zd)",
			sizeof(*to), r);
	if (lseek(integrity_fd(c), orig_pos, SEEK_SET) != orig_pos)
		die_errno("seek to original position failed (integrity file)");
}

static void indices_open(struct journal_ctx *c)
{
	c->extents_lock = xcalloc(1, sizeof(*c->extents_lock));
	c->integrity_lock = xcalloc(1, sizeof(*c->integrity_lock));

	lock_for_safeappend_with_delayed_retry(c->extents_lock,
					       journal_extents_file());

	lock_for_safeappend_with_delayed_retry(c->integrity_lock,
					       journal_integrity_file());
}

static void indices_close(struct journal_ctx *c)
{
	struct journal_extent_rec extent;
	struct journal_integrity_rec integrity, last_irec;
	size_t i;
	size_t total_bytes = 0;
	uint32_t self_crc;
	uint32_t extent_data_crc;

	if (!c->index_entries.nr) {
		/* No new indices. */
		return;
	}

	printf(Q_("Recording %d extensions\n", "Recording %d extension", c->index_entries.nr), (int)c->index_entries.nr);

	/* Load last integrity record for use in CRC computation */

	integrity_last_record(c, &last_irec);
	journal_integrity_record_from_wire(&last_irec);
	self_crc = last_irec.self_crc;
	extent_data_crc = last_irec.extent_data_crc;

	for (i = 0; i < c->index_entries.nr; ++i) {
		struct index_entry_t *index_entry = &c->index_entries.items[i];
		off_t extents_offset, integrity_offset;
		size_t extent_no, integrity_no;

		total_bytes += index_entry->extent.length;

		memcpy(&extent, &index_entry->extent, sizeof(extent));
		journal_extent_record_to_wire(&extent);
		extent_data_crc = crc32(extent_data_crc,
					(const unsigned char *)&extent,
					sizeof(extent));
		index_entry->integrity.extent_data_crc = extent_data_crc;
		memcpy(&integrity, &index_entry->integrity, sizeof(integrity));
		journal_integrity_record_to_wire(&integrity);
		self_crc = crc32(0,
				(const unsigned char *)&integrity + IREC_DATA_OFFSET,
				IREC_DATA_SIZE);
		integrity.self_crc = htonl(self_crc);

		extents_offset = lseek(extents_fd(c), 0, SEEK_END);
		integrity_offset = lseek(integrity_fd(c), 0, SEEK_END);

		extent_no = extents_offset / sizeof(extent);
		integrity_no = integrity_offset / sizeof(last_irec);
		if (c->use_integrity && (extent_no != integrity_no)) {
			die("disparity in extent record count (%zu) and integrity record count (%zu)",
					extent_no, integrity_no);
		}
		if (write_in_full(extents_fd(c), &extent, sizeof(extent)) != sizeof(extent)) {
			die_errno("write failed: extent data");
		}
		if (c->use_integrity && (write_in_full(integrity_fd(c), &integrity, sizeof(integrity)) != sizeof(integrity))) {
			die_errno("write failed: integrity data");
		}
	}

	if (xfsync(extents_fd(c)) != 0)
		die_errno("fsync extents file failed");
	if (c->use_integrity && (xfsync(integrity_fd(c)) != 0))
		die_errno("fsync integrity file failed");

	commit_locked_safeappend_file(c->extents_lock);
	commit_locked_safeappend_file(c->integrity_lock);

	printf("Extents flushed for %zuKB of journaled data\n",
	       (total_bytes / KILOBYTE_BYTES));
}

static const char *journal_op_name(const unsigned char op)
{
	if (op == JOURNAL_OP_PACK)
		return "pack";
	else if (op == JOURNAL_OP_INDEX)
		return "index";
	else if (op == JOURNAL_OP_REF)
		return "ref";
	else if (op == JOURNAL_OP_UPGRADE)
		return "upgrade";
	else
		return "(unknown)";
};

static void journal_write_content(struct journal_ctx *c,
				  const struct journal *j,
				  const unsigned char opcode,
				  const unsigned char *sha1,
				  const uint32_t payload_len,
				  void *payload)
{
	/* Create the index record. */
	struct index_entry_t *index_entry = index_entry_new(c);
	struct journal_header header = {0};
	git_SHA_CTX shactx;

	index_entry->extent.opcode = opcode;
	index_entry->extent.serial = j->serial;
	index_entry->extent.offset = lseek(j->fd, 0, SEEK_CUR);

	/* Create the header. */

	if (memcpy(&header.opcode, &opcode, sizeof(opcode)) != &header.opcode)
		die_errno("journal entry header assembly failed: opcode");
	if (memcpy(&header.sha, sha1, 20) != &header.sha)
		die_errno("journal entry header assembly failed: sha");
	if (memcpy(&header.payload_length, &payload_len, sizeof(payload_len)) != &header.payload_length) {
		die_errno("journal entry header assembly failed: payload length");
	}
	journal_header_to_wire(&header);

	/* Write the header and the payload, add the checksum to the integrity data. */

	git_SHA1_Init(&shactx);
	if (write_in_full(j->fd, &header, sizeof(header)) != sizeof(header))
		die_errno("write failed: journal header");
	git_SHA1_Update(&shactx, &header, sizeof(header));
	if (write_in_full(j->fd, payload, payload_len) != payload_len)
		die_errno("write failed: journal payload");
	git_SHA1_Update(&shactx, payload, payload_len);
	git_SHA1_Final(index_entry->integrity.data_sha1, &shactx);

	/* Update the extent record to include the length. */
	index_entry->extent.length = sizeof(header) + payload_len;

	printf("Journaled %5s %"PRIx32"@%"PRIu32"+%"PRIu32"\n",
	       journal_op_name(index_entry->extent.opcode),
	       index_entry->extent.serial,
	       index_entry->extent.offset,
	       index_entry->extent.length);
}

static size_t journal_record_len(size_t len) {
	return xsize_t(sizeof(struct journal_header) + len);
}

void journal_write_tip(struct journal_ctx *c,
		       const char *ref_name,
		       const unsigned char *tip_sha1)
{
	const size_t ref_name_len = xsize_t(strlen(ref_name));
	const size_t ref_rec_len = journal_record_len(ref_name_len);
	struct journal *j = journal_with_capacity(c, ref_rec_len);

	trace_printf("Appending ref '%s'-> %s\n",
		     ref_name, sha1_to_hex(tip_sha1));
	journal_write_content(c, j, JOURNAL_OP_REF, tip_sha1, ref_name_len, (void *)ref_name);
	journal_close(c, j);
}

void journal_wire_version_print(const struct journal_wire_version *version)
{
	printf("%s = { version: %"PRIu32" }\n",
	       sha1_to_hex((unsigned char *)version),
	       htons(version->version));
}

void journal_write_upgrade(struct journal_ctx *c, const struct journal_wire_version *version)
{
	const size_t upgrade_rec_len = journal_record_len(0);
	struct journal *j = journal_with_capacity(c, upgrade_rec_len);

	trace_printf("Appending upgrade '%s'\n",
		     sha1_to_hex((unsigned char *)version));
	journal_write_content(c, j, JOURNAL_OP_UPGRADE, (unsigned char *)version, 0, (void *)version);
	journal_close(c, j);
}

void journal_write_pack(struct journal_ctx *c,
			struct packed_git *pack,
			const size_t max_pack_size)
{
	struct stat sb_pack;
	struct stat sb_index;
	const char *pack_name = sha1_pack_name(pack->sha1);
	const char *pack_index_name;
	size_t pack_len, pack_index_len, pack_rec_len;
	struct journal *j;
	int open_flags, pack_fd, index_fd;
	void *map;

	if (stat(pack_name, &sb_pack) != 0)
		die_errno("stat pack failed");

	if (!has_pack_index(pack->sha1))
		die("no pack index for pack %s",
		    sha1_to_hex(pack->sha1));

	pack_index_name = sha1_pack_index_name(pack->sha1);
	if (stat(pack_index_name, &sb_index) != 0)
		die_errno("stat index failed");

	pack_len = xsize_t(sb_pack.st_size);
	pack_index_len = xsize_t(sb_index.st_size);
	pack_rec_len = journal_record_len(pack_len + pack_index_len);

	if (max_pack_size > 0 && pack_len > max_pack_size) {
		die("pack size %lu is greater than administrative maximum size of %lu, refusing to add",
				pack_len, max_pack_size);
	}

	trace_printf("Appending pack %s (pack %zuB + index %zuB)\n",
		     sha1_to_hex(pack->sha1), pack_len, pack_index_len);

	j = journal_with_capacity(c, pack_rec_len);

	open_flags = O_RDONLY;
#ifdef O_LARGEFILE
	open_flags |= O_LARGEFILE;
#endif

	pack_fd = open(pack_name, open_flags);
	if (pack_fd < 0)
		die_errno("open pack failed");

	map = xmmap(NULL, pack_len, PROT_READ, MAP_PRIVATE, pack_fd, 0);
	journal_write_content(c, j, JOURNAL_OP_PACK, pack->sha1, pack_len, map);
	munmap(map, pack_len);
	close(pack_fd);

	index_fd = open(pack_index_name, open_flags);
	if (index_fd < 0)
		die_errno("open index failed");
	map = xmmap(NULL, pack_len, PROT_READ, MAP_PRIVATE, index_fd, 0);
	journal_write_content(c, j, JOURNAL_OP_INDEX, pack->sha1, pack_index_len, map);
	munmap(map, pack_index_len);
	close(index_fd);

	journal_close(c, j);
}

struct packed_git *journal_locate_pack_by_sha(const unsigned char *sha1)
{
	prepare_packed_git();
	return find_sha1_pack(sha1, packed_git);
}

struct journal_ctx *journal_ctx_open(size_t size_limit, int use_integrity)
{
	struct journal_ctx *c;
	c = xmalloc(sizeof(*c));
	memset(c, 0, sizeof(*c));

	journal_setup(c);
	metadata_load(&c->meta);
	indices_open(c);
	c->size_limit = size_limit;
	c->use_integrity = use_integrity;

	return c;
}

static void state_store(void)
{
	static struct lock_file lock;
	struct stat st;
	int fd;
	uint32_t state;

	if (stat(journal_extents_file(), &st))
		die("Failed to read extents file");
	state = htonl(st.st_size);

	fd = hold_lock_file_for_update(&lock, journal_state_file(), LOCK_DIE_ON_ERROR);
	if (fd < 0)
		die("Failed to lock state file\n");
	write_in_full(fd, &state, 4);
	commit_lock_file(&lock);
}

void journal_ctx_close(struct journal_ctx *c)
{
	metadata_store(&c->meta);
	indices_close(c);
	state_store();
	free(c);
}

/**
 * Serialize a struct journal_header.
 */
void journal_header_to_wire(struct journal_header *header)
{
	header->payload_length = htonl(header->payload_length);
}

/**
 * Deserialize a struct journal_header./
 */
void journal_header_from_wire(struct journal_header *header)
{
	header->payload_length = ntohl(header->payload_length);
}

unsigned long journal_size_limit_from_config(void)
{
	return git_config_ulong("journal.size-limit", JOURNAL_MAX_SIZE_DEFAULT);
}

int journal_integrity_from_config(void)
{
	return git_config_bool("journal.integrity", NULL);
}

const struct journal_wire_version * const journal_wire_version(void)
{
	static int initialized = 0;
	static struct journal_wire_version v;

	if (!initialized) {
		memset(&v, 0, sizeof(v));
		v.version = htons(JOURNAL_WIRE_VERSION);
		++initialized;
	}
	return &v;
}

void journal_extent_record_to_wire(struct journal_extent_rec *e)
{
	e->serial = htonl(e->serial);
	e->offset = htonl(e->offset);
	e->length = htonl(e->length);
}

void journal_extent_record_from_wire(struct journal_extent_rec *e)
{
	e->serial = ntohl(e->serial);
	e->offset = ntohl(e->offset);
	e->length = ntohl(e->length);
}

void journal_integrity_record_to_wire(struct journal_integrity_rec *r)
{
	r->self_crc = htonl(r->self_crc);
	r->extent_data_crc = htonl(r->extent_data_crc);
}

void journal_integrity_record_from_wire(struct journal_integrity_rec *r)
{
	r->self_crc = ntohl(r->self_crc);
	r->extent_data_crc = ntohl(r->extent_data_crc);
}

struct packed_git *journal_find_pack(const unsigned char *sha1)
{
	struct packed_git *p;
	struct strbuf sb = STRBUF_INIT;
	const char *objdir;

	prepare_packed_git();

	for (p = packed_git; p; p = p->next) {
		if (!hashcmp(p->sha1, sha1))
			return p;
	}

	/*
	 * The pack might not have existsed the first time we called
	 * prepare_packed_git(), so let's try to load it.
	 */
	objdir = get_object_directory();
	strbuf_addf(&sb, "%s/pack/pack-%s.idx", objdir, sha1_to_hex(sha1));
	p = add_packed_git(sb.buf, sb.len, 0);

	if (!p)
		die("no such pack %s", sha1_to_hex(sha1));

	install_packed_git(p);

	strbuf_release(&sb);
	return p;
}
