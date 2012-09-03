/* Index extensions.
 *
 * The first letter should be 'A'..'Z' for extensions that are not
 * necessary for a correct operation (i.e. optimization data).
 * When new extensions are added that _needs_ to be understood in
 * order to correctly interpret the index file, pick character that
 * is outside the range, to cause the reader to abort.
 */

#define CACHE_EXT(s) ( (s[0]<<24)|(s[1]<<16)|(s[2]<<8)|(s[3]) )
#define CACHE_EXT_TREE 0x54524545	/* "TREE" */
#define CACHE_EXT_RESOLVE_UNDO 0x52455543 /* "REUC" */

#define INDEX_FORMAT_DEFAULT 3

/*
 * Basic data structures for the directory cache
 */
struct cache_version_header {
	unsigned int hdr_signature;
	unsigned int hdr_version;
};

struct index_ops {
	int (*match_stat_basic)(struct cache_entry *ce, struct stat *st, int changed);
	int (*verify_hdr)(void *mmap, unsigned long size);
	int (*read_index)(struct index_state *istate, void *mmap, int mmap_size);
	int (*write_index)(struct index_state *istate, int newfd);
};

extern struct index_ops v2_ops;
extern struct index_ops v5_ops;

#ifndef NEEDS_ALIGNED_ACCESS
#define ntoh_s(var) ntohs(var)
#define ntoh_l(var) ntohl(var)
#else
static inline uint16_t ntoh_s_force_align(void *p)
{
	uint16_t x;
	memcpy(&x, p, sizeof(x));
	return ntohs(x);
}
static inline uint32_t ntoh_l_force_align(void *p)
{
	uint32_t x;
	memcpy(&x, p, sizeof(x));
	return ntohl(x);
}
#define ntoh_s(var) ntoh_s_force_align(&(var))
#define ntoh_l(var) ntoh_l_force_align(&(var))
#endif

extern int ce_modified_check_fs(struct cache_entry *ce, struct stat *st);
extern int ce_match_stat_basic(struct index_state *istate,
		struct cache_entry *ce, struct stat *st);
extern int is_racy_timestamp(const struct index_state *istate, struct cache_entry *ce);
extern void set_index_entry(struct index_state *istate, int nr, struct cache_entry *ce);
extern uint32_t calculate_stat_crc(struct cache_entry *ce);
extern void set_istate_ops(struct index_state *istate);
