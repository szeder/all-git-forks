/*
 * Copyright (c) 2013 Ramkumar Ramachandra
 */
#include "cache.h"
#include "link.h"
#include "cache-tree.h"

int cmd_edit_link(int argc, const char **argv, const char *prefix)
{
	static struct lock_file lock_file;
	int newfd;
	const char *path;
	struct stat st;
	struct cache_entry *ce, *ent;
	struct strbuf sb = STRBUF_INIT;
	int namelen, size, pos;

	if (argc < 2)
		die("Usage: git edit-link <link>");
	path = argv[1];

	/* Lock the index for update */
	newfd = hold_locked_index(&lock_file, 1);

	/* Read in the current index */
	if (read_cache() < 0)
		die(_("index file corrupt"));

	/* Prepare the cache_entry */
	if (lstat(path, &st))
		die_errno("unable to stat '%s'", path);
	namelen = strlen(path);
	size = cache_entry_size(namelen);
	ce = xcalloc(1, size);
	memcpy(ce->name, path, namelen);
	ce->ce_namelen = namelen;
	fill_stat_cache_info(ce, &st);

	pos = index_name_pos_also_unmerged(&the_index, path, namelen);
	ent = (0 <= pos) ? the_index.cache[pos] : NULL;
	ce->ce_mode = ce_mode_from_stat(ent, st.st_mode);

	/* Write the new link object to database */
	strbuf_addf(&sb, "upstream_url = %s\n", "gh:artagnon/quux");
	strbuf_addf(&sb, "checkout_rev = %s\n", "refs/heads/master");
	strbuf_addf(&sb, "ref_name = %s\n", "quux");
	if (write_sha1_file(sb.buf, sb.len, link_type, ce->sha1))
		return error("%s: failed to insert into database", path);
	strbuf_release(&sb);

	/* Now add the entry to the index, and write it out */
	if (add_index_entry(&the_index, ce,
				    ADD_CACHE_OK_TO_ADD|ADD_CACHE_OK_TO_REPLACE))
		return error("unable to add %s to index", path);
	if (write_cache(newfd, active_cache, active_nr) ||
		commit_locked_index(&lock_file))
		die(_("Unable to write new index file"));
	
	return 0;
}
