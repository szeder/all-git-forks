#ifndef JOURNAL_H
#define JOURNAL_H

#include "cache.h"
#include "remote.h"

static const size_t journal_buf_size = 65536;
#define GLUE(a,b) __GLUE(a,b)
#define __GLUE(a,b) a ## b

#define CVERIFY(expr, msg) typedef char GLUE (compiler_verify_, msg) [(expr) ? (+1) : (-1)]

#define COMPILER_VERIFY(exp) CVERIFY (exp, __LINE__)

static const unsigned char JOURNAL_OP_PACK = 'p';
static const unsigned char JOURNAL_OP_INDEX = 'i';
static const unsigned char JOURNAL_OP_REF = 'r';
static const unsigned char JOURNAL_OP_UPGRADE = 'V';

#define JOURNAL_MAX_SIZE_DEFAULT "3g"
#define JOURNAL_WIRE_VERSION 1

/* Disk formats */
struct journal_wire_version {
	uint16_t version;
	unsigned char reserved_space[18]; /* For future use. We need 20 bytes anyway. */
};
COMPILER_VERIFY(sizeof(struct journal_wire_version) == 20);

struct journal_header {
	unsigned char opcode;
	unsigned char sha[20];
	uint32_t payload_length;
};
COMPILER_VERIFY(sizeof(struct journal_header) == 28);

struct journal_metadata {
	char magic[2];
	char reserved[2];
	uint32_t journal_serial;
};
COMPILER_VERIFY(sizeof(struct journal_metadata) == 8);

struct journal_extent_rec {
	uint32_t serial;
	unsigned char opcode;
	char reserved[3];
	uint32_t offset;
	uint32_t length;
};
COMPILER_VERIFY(sizeof(struct journal_extent_rec) == 16);

struct journal_integrity_rec {
	uint32_t self_crc;
	uint32_t extent_data_crc;
	unsigned char data_sha1[20];
	char reserved[4];
};
COMPILER_VERIFY(sizeof(struct journal_integrity_rec) == 32);

struct remote_state_rec {
	uint32_t processed_offset;
};

COMPILER_VERIFY(sizeof(struct remote_state_rec) == 4);

static const size_t IREC_DATA_OFFSET = offsetof(struct journal_integrity_rec, extent_data_crc);
static const size_t IREC_DATA_SIZE = sizeof(struct journal_integrity_rec) - offsetof(struct journal_integrity_rec, extent_data_crc);

struct index_entry_t {
	struct journal_extent_rec extent;
	struct journal_integrity_rec integrity;
};

struct index_entries {			\
	size_t nr;			\
	size_t alloc;				\
	struct index_entry_t * items;		\
};

struct journal_ctx {
	struct journal_metadata meta;
	size_t size_limit;
	struct safeappend_lock *extents_lock;
	struct safeappend_lock *integrity_lock;
	struct index_entries index_entries;
	unsigned char use_integrity:1;
};

struct journal_ctx *journal_ctx_open(size_t size_limit, int use_integrity);
void journal_ctx_close(struct journal_ctx *c);

struct packed_git *journal_locate_pack_by_sha(const unsigned char *sha1);

void journal_write_pack(struct journal_ctx *c,
		struct packed_git *pack,
		const unsigned long max_pack_size);

void journal_write_tip(struct journal_ctx *c,
		const char *ref_name,
		const unsigned char *commit_sha1);

void journal_write_upgrade(struct journal_ctx *c,
		const struct journal_wire_version *version);

void journal_header_to_wire(struct journal_header *header);
void journal_header_from_wire(struct journal_header *header);
unsigned long journal_size_limit_from_config(void);
int journal_integrity_from_config(void);
const struct journal_wire_version * const journal_wire_version(void);
void journal_wire_version_print(const struct journal_wire_version *version);

void journal_extent_record_to_wire(struct journal_extent_rec *e);
void journal_extent_record_from_wire(struct journal_extent_rec *e);
void journal_integrity_record_to_wire(struct journal_integrity_rec *t);
void journal_integrity_record_from_wire(struct journal_integrity_rec *t);
void journal_metadata_store(const char *path, struct journal_metadata *m);

struct packed_git *journal_find_pack(const unsigned char *sha1);

const char *journal_dir(void);
const char *journal_remote_dir(const struct remote * const upstream);

#endif
