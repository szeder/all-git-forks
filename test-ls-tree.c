#include "cache.h"
#include "pack.h"
#include "pack-revindex.h"
#include "progress.h"
#include "varint.h"
#include "packv4-create.h"
#include "tree-walk.h"
#include "packv4-parse.h"

static char *normalize_pack_name(const char *path)
{
	char buf[PATH_MAX];
	int len;

	len = strlcpy(buf, path, PATH_MAX);
	if (len >= PATH_MAX - 6)
		die("name too long: %s", path);

	/*
	 * In addition to "foo.idx" we accept "foo.pack" and "foo";
	 * normalize these forms to "foo.pack".
	 */
	if (ends_with(buf, ".idx")) {
		strcpy(buf + len - 4, ".pack");
		len++;
	} else if (!ends_with(buf, ".pack")) {
		strcpy(buf + len, ".pack");
		len += 5;
	}

	return xstrdup(buf);
}

static struct packed_git *open_pack(const char *path)
{
	char *packname = normalize_pack_name(path);
	int len = strlen(packname);
	struct packed_git *p;

	strcpy(packname + len - 5, ".idx");
	p = add_packed_git(packname, len - 1, 1);
	if (!p)
		die("packfile %s not found.", packname);

	install_packed_git(p);
	if (open_pack_index(p))
		die("packfile %s index not opened", p->pack_name);
	if (!is_pack_valid(p) || p->version != 4)
		die("input pack is not v4");

	free(packname);
	return p;
}

int main(int ac, char **av)
{
	unsigned char sha1[20];
	struct decode_state ds, *dsp;
	struct pack_window *w_curs = NULL;
	int ret, entry_nr;

	if (ac != 3)
		die("needs pack name and tree sha1");

	memset(&ds, 0, sizeof(ds));
	dsp = &ds;
	ds.p = open_pack(av[1]);
	ds.w_curs = &w_curs;
	if (!get_sha1_hex(av[2], sha1))
		ds.obj_offset = find_pack_entry_one(sha1, ds.p);
	else
		ds.obj_offset = strtoul(av[2], NULL, 0);
	if (!ds.obj_offset)
		die("tree not in pack");

	entry_nr = 0;
	while ((ret = decode_tree_entry(&dsp)) != decode_done) {
		struct decode_state *p = dsp;
		int nr_hops = 0;
		if (ret == decode_failed)
			die("failed to decode");
		if (ret == fallingback)
			die("not support referencing to tree v2");

		printf("[% 3d] sha1 = %s path_index = % 5d obj_offset = %lu pos = %u %u/%u/%u/%lu",
		       entry_nr, sha1_to_hex(dsp->sha1), dsp->path_index,
		       dsp->obj_offset, dsp->curpos - 1,
		       dsp->curpos, dsp->skip, dsp->count, ds.offset - ds.obj_offset);

		while (p != &ds) {
			p = p->up;
			if (!(nr_hops++ % 6))
				printf("\n     ");
			printf(" <- %lu(%u/%u/%u)", p->obj_offset, p->curpos,
			       p->skip, p->count);
		}
		printf("\n");
		entry_nr++;
	}
	printf("tree size %lu bytes\n", ds.offset - ds.obj_offset);
	return 0;
}
