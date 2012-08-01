#include "cache.h"
#include "dir.h"
#include "resolve-undo.h"
#include "string-list.h"

/* The only error case is to run out of memory in string-list */
void record_resolve_undo(struct index_state *istate, struct cache_entry *ce)
{
	struct string_list_item *lost;
	struct resolve_undo_info *ui;
	struct string_list *resolve_undo;
	int stage = ce_stage(ce);

	if (!stage)
		return;

	if (!istate->resolve_undo) {
		resolve_undo = xcalloc(1, sizeof(*resolve_undo));
		resolve_undo->strdup_strings = 1;
		istate->resolve_undo = resolve_undo;
	}
	resolve_undo = istate->resolve_undo;
	lost = string_list_insert(resolve_undo, ce->name);
	if (!lost->util)
		lost->util = xcalloc(1, sizeof(*ui));
	ui = lost->util;
	hashcpy(ui->sha1[stage - 1], ce->sha1);
	ui->mode[stage - 1] = ce->ce_mode;
}

void resolve_undo_write(struct strbuf *sb, struct string_list *resolve_undo)
{
	struct string_list_item *item;
	for_each_string_list_item(item, resolve_undo) {
		struct resolve_undo_info *ui = item->util;
		int i;

		if (!ui)
			continue;
		strbuf_addstr(sb, item->string);
		strbuf_addch(sb, 0);
		for (i = 0; i < 3; i++)
			strbuf_addf(sb, "%o%c", ui->mode[i], 0);
		for (i = 0; i < 3; i++) {
			if (!ui->mode[i])
				continue;
			strbuf_add(sb, ui->sha1[i], 20);
		}
	}
}

struct string_list *resolve_undo_read(const char *data, unsigned long size)
{
	struct string_list *resolve_undo;
	size_t len;
	char *endptr;
	int i;

	resolve_undo = xcalloc(1, sizeof(*resolve_undo));
	resolve_undo->strdup_strings = 1;

	while (size) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;

		len = strlen(data) + 1;
		if (size <= len)
			goto error;
		lost = string_list_insert(resolve_undo, data);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;
		size -= len;
		data += len;

		for (i = 0; i < 3; i++) {
			ui->mode[i] = strtoul(data, &endptr, 8);
			if (!endptr || endptr == data || *endptr)
				goto error;
			len = (endptr + 1) - (char*)data;
			if (size <= len)
				goto error;
			size -= len;
			data += len;
		}

		for (i = 0; i < 3; i++) {
			if (!ui->mode[i])
				continue;
			if (size < 20)
				goto error;
			hashcpy(ui->sha1[i], (const unsigned char *)data);
			size -= 20;
			data += 20;
		}
	}
	return resolve_undo;

error:
	string_list_clear(resolve_undo, 1);
	error("Index records invalid resolve-undo information");
	return NULL;
}

void resolve_undo_clear_index(struct index_state *istate)
{
	struct string_list *resolve_undo = istate->resolve_undo;
	if (!resolve_undo)
		return;
	string_list_clear(resolve_undo, 1);
	free(resolve_undo);
	istate->resolve_undo = NULL;
	istate->cache_changed = 1;
}

int unmerge_index_entry_at(struct index_state *istate, int pos)
{
	struct cache_entry *ce;
	struct string_list_item *item;
	struct resolve_undo_info *ru;
	int i, err = 0;

	if (!istate->resolve_undo)
		return pos;

	ce = istate->cache[pos];
	if (ce_stage(ce)) {
		/* already unmerged */
		while ((pos < istate->cache_nr) &&
		       ! strcmp(istate->cache[pos]->name, ce->name))
			pos++;
		return pos - 1; /* return the last entry processed */
	}
	item = string_list_lookup(istate->resolve_undo, ce->name);
	if (!item)
		return pos;
	ru = item->util;
	if (!ru)
		return pos;
	remove_index_entry_at(istate, pos);
	for (i = 0; i < 3; i++) {
		struct cache_entry *nce;
		if (!ru->mode[i])
			continue;
		nce = make_cache_entry(ru->mode[i], ru->sha1[i],
				       ce->name, i + 1, 0);
		if (add_index_entry(istate, nce, ADD_CACHE_OK_TO_ADD)) {
			err = 1;
			error("cannot unmerge '%s'", ce->name);
		}
	}
	if (err)
		return pos;
	free(ru);
	item->util = NULL;
	return unmerge_index_entry_at(istate, pos);
}

void unmerge_index(struct index_state *istate, const char **pathspec)
{
	int i;

	if (!istate->resolve_undo)
		return;

	for (i = 0; i < istate->cache_nr; i++) {
		struct cache_entry *ce = istate->cache[i];
		if (!match_pathspec(pathspec, ce->name, ce_namelen(ce), 0, NULL))
			continue;
		i = unmerge_index_entry_at(istate, i);
	}
}

void resolve_undo_convert_v5(struct index_state *istate,
					struct conflict_entry *ce)
{
	int i;

	while (ce) {
		struct string_list_item *lost;
		struct resolve_undo_info *ui;
		struct conflict_part *cp;

		if (ce->entries && (ce->entries->flags & CONFLICT_CONFLICTED) != 0) {
			ce = ce->next;
			continue;
		}
		if (!istate->resolve_undo) {
			istate->resolve_undo = xcalloc(1, sizeof(struct string_list));
			istate->resolve_undo->strdup_strings = 1;
		}

		lost = string_list_insert(istate->resolve_undo, ce->name);
		if (!lost->util)
			lost->util = xcalloc(1, sizeof(*ui));
		ui = lost->util;

		cp = ce->entries;
		for (i = 0; i < 3; i++)
			ui->mode[i] = 0;
		while (cp) {
			ui->mode[conflict_stage(cp) - 1] = cp->entry_mode;
			hashcpy(ui->sha1[conflict_stage(cp) - 1], cp->sha1);
			cp = cp->next;
		}
		ce = ce->next;
	}
}

void resolve_undo_to_ondisk_v5(struct hash_table *table,
				struct string_list *resolve_undo,
				unsigned int *ndir, int *total_dir_len,
				struct directory_entry *de)
{
	struct string_list_item *item;
	struct directory_entry *search;

	if (!resolve_undo)
		return;
	for_each_string_list_item(item, resolve_undo) {
		struct conflict_entry *conflict_entry;
		struct resolve_undo_info *ui = item->util;
		char *super;
		int i, dir_len, len;
		uint32_t crc;
		struct directory_entry *found, *current, *new_tree;

		if (!ui)
			continue;

		super = super_directory(item->string);
		if (!super)
			dir_len = 0;
		else
			dir_len = strlen(super);
		crc = crc32(0, (Bytef*)super, dir_len);
		found = lookup_hash(crc, table);
		current = NULL;
		new_tree = NULL;
		
		while (!found) {
			struct directory_entry *new;

			new = init_directory_entry(super, dir_len);
			if (!current)
				current = new;
			insert_directory_entry(new, table, total_dir_len, ndir, crc);
			if (new_tree != NULL)
				new->de_nsubtrees = 1;
			new->next = new_tree;
			new_tree = new;
			super = super_directory(super);
			if (!super)
				dir_len = 0;
			else
				dir_len = strlen(super);
			crc = crc32(0, (Bytef*)super, dir_len);
			found = lookup_hash(crc, table);
		}
		search = found;
		while (search->next_hash && strcmp(super, search->pathname) != 0)
			search = search->next_hash;
		if (search && !current)
			current = search;
		if (!search && !current)
			current = new_tree;
		if (!super && new_tree) {
			new_tree->next = de->next;
			de->next = new_tree;
			de->de_nsubtrees++;
		} else if (new_tree) {
			struct directory_entry *temp;

			search = de->next;
			while (strcmp(super, search->pathname))
				search = search->next;
			temp = new_tree;
			while (temp->next)
				temp = temp->next;
			search->de_nsubtrees++;
			temp->next = search->next;
			search->next = new_tree;
		}

		len = strlen(item->string);
		conflict_entry = create_new_conflict(item->string, len, current->de_pathlen);
		add_conflict_to_directory_entry(current, conflict_entry);
		for (i = 0; i < 3; i++) {
			if (ui->mode[i]) {
				struct conflict_part *cp;

				cp = xmalloc(sizeof(struct conflict_part));
				cp->flags = (i + 1) << CONFLICT_STAGESHIFT;
				cp->entry_mode = ui->mode[i];
				cp->next = NULL;
				hashcpy(cp->sha1, ui->sha1[i]);
				add_part_to_conflict_entry(current, conflict_entry, cp);
			}
		}
	}
}
