#include "cache.h"
#include "pack-revindex.h"

/*
 * Pack index for existing packs give us easy access to the offsets into
 * corresponding pack file where each object's data starts, but the entries
 * do not store the size of the compressed representation (uncompressed
 * size is easily available by examining the pack entry header).  It is
 * also rather expensive to find the sha1 for an object given its offset.
 *
 * We build a hashtable of existing packs (pack_revindex), and keep reverse
 * index here -- pack index file is sorted by object name mapping to offset;
 * this pack_revindex[].revindex array is a list of offset/index_nr pairs
 * ordered by offset, so if you know the offset of an object, next offset
 * is where its packed representation ends and the index_nr can be used to
 * get the object sha1 from the main index.
 */

struct pack_revindex {
	struct packed_git *p;
	struct revindex_entry *revindex;
};

static struct pack_revindex *pack_revindex;
static int pack_revindex_hashsz;

static int pack_revindex_ix(struct packed_git *p)
{
	unsigned long ui = (unsigned long)p;
	int i;

	ui = ui ^ (ui >> 16); /* defeat structure alignment */
	i = (int)(ui % pack_revindex_hashsz);
	while (pack_revindex[i].p) {
		if (pack_revindex[i].p == p)
			return i;
		if (++i == pack_revindex_hashsz)
			i = 0;
	}
	return -1 - i;
}

static void init_pack_revindex(void)
{
	int num;
	struct packed_git *p;

	for (num = 0, p = packed_git; p; p = p->next)
		num++;
	if (!num)
		return;
	pack_revindex_hashsz = num * 11;
	pack_revindex = xcalloc(sizeof(*pack_revindex), pack_revindex_hashsz);
	for (p = packed_git; p; p = p->next) {
		num = pack_revindex_ix(p);
		num = - 1 - num;
		pack_revindex[num].p = p;
	}
	/* revindex elements are lazily initialized */
}

/*
 * This is a least-significant-digit radix sort using a 16-bit "digit".
 */
static void sort_revindex(struct revindex_entry *entries, int n, off_t max)
{
	/*
	 * We need O(n) temporary storage, so we sort back and forth between
	 * the real array and our tmp storage. To keep them straight, we always
	 * sort from "a" into buckets in "b".
	 */
	struct revindex_entry *tmp = xcalloc(n, sizeof(*tmp));
	struct revindex_entry *a = entries, *b = tmp;
	int digits = 0;

	/*
	 * We want to know the bucket that a[i] will go into when we are using
	 * the digit that is N bits from the (least significant) end.
	 */
#define BUCKET_FOR(a, i, digits) ((a[i].offset >> digits) & 0xffff)

	while (max / (((off_t)1) << digits)) {
		struct revindex_entry *swap;
		int i;
		int pos[65536] = {0};

		/*
		 * We want pos[i] to store the index of the last element that
		 * will go in bucket "i" (actually one past the last element).
		 * To do this, we first count the items that will go in each
		 * bucket, which gives us a relative offset from the last
		 * bucket. We can then cumulatively add the index from the
		 * previous bucket to get the true index.
		 */
		for (i = 0; i < n; i++)
			pos[BUCKET_FOR(a, i, digits)]++;
		for (i = 1; i < ARRAY_SIZE(pos); i++)
			pos[i] += pos[i-1];

		/*
		 * Now we can drop the elements into their correct buckets (in
		 * our temporary array).  We iterate the pos counter backwards
		 * to avoid using an extra index to count up. And since we are
		 * going backwards there, we must also go backwards through the
		 * array itself, to keep the sort stable.
		 */
		for (i = n - 1; i >= 0; i--)
			b[--pos[BUCKET_FOR(a, i, digits)]] = a[i];

		/*
		 * Now "b" contains the most sorted list, so we swap "a" and
		 * "b" for the next iteration.
		 */
		swap = a;
		a = b;
		b = swap;

		/* And bump our digits for the next round. */
		digits += 16;
	}

	/*
	 * If we ended with our data in the original array, great. If not,
	 * we have to move it back from the temporary storage.
	 */
	if (a != entries) {
		int i;
		for (i = 0; i < n; i++)
			entries[i] = tmp[i];
	}
	free(tmp);

#undef BUCKET_FOR
}

/*
 * Ordered list of offsets of objects in the pack.
 */
static void create_pack_revindex(struct pack_revindex *rix)
{
	struct packed_git *p = rix->p;
	int num_ent = p->num_objects;
	int i;
	const char *index = p->index_data;

	rix->revindex = xmalloc(sizeof(*rix->revindex) * (num_ent + 1));
	index += 4 * 256;

	if (p->index_version > 1) {
		const uint32_t *off_32 =
			(uint32_t *)(index + 8 + p->num_objects * (20 + 4));
		const uint32_t *off_64 = off_32 + p->num_objects;
		for (i = 0; i < num_ent; i++) {
			uint32_t off = ntohl(*off_32++);
			if (!(off & 0x80000000)) {
				rix->revindex[i].offset = off;
			} else {
				rix->revindex[i].offset =
					((uint64_t)ntohl(*off_64++)) << 32;
				rix->revindex[i].offset |=
					ntohl(*off_64++);
			}
			rix->revindex[i].nr = i;
		}
	} else {
		for (i = 0; i < num_ent; i++) {
			uint32_t hl = *((uint32_t *)(index + 24 * i));
			rix->revindex[i].offset = ntohl(hl);
			rix->revindex[i].nr = i;
		}
	}

	/* This knows the pack format -- the 20-byte trailer
	 * follows immediately after the last object data.
	 */
	rix->revindex[num_ent].offset = p->pack_size - 20;
	rix->revindex[num_ent].nr = -1;
	sort_revindex(rix->revindex, num_ent, p->pack_size);
}

struct revindex_entry *find_pack_revindex(struct packed_git *p, off_t ofs)
{
	int num;
	int lo, hi;
	struct pack_revindex *rix;
	struct revindex_entry *revindex;

	if (!pack_revindex_hashsz)
		init_pack_revindex();
	num = pack_revindex_ix(p);
	if (num < 0)
		die("internal error: pack revindex fubar");

	rix = &pack_revindex[num];
	if (!rix->revindex)
		create_pack_revindex(rix);
	revindex = rix->revindex;

	lo = 0;
	hi = p->num_objects + 1;
	do {
		int mi = (lo + hi) / 2;
		if (revindex[mi].offset == ofs) {
			return revindex + mi;
		} else if (ofs < revindex[mi].offset)
			hi = mi;
		else
			lo = mi + 1;
	} while (lo < hi);
	error("bad offset for revindex");
	return NULL;
}

void discard_revindex(void)
{
	if (pack_revindex_hashsz) {
		int i;
		for (i = 0; i < pack_revindex_hashsz; i++)
			free(pack_revindex[i].revindex);
		free(pack_revindex);
		pack_revindex_hashsz = 0;
	}
}
