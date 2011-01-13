/*
 * Licensed under a two-clause BSD-style license.
 * See LICENSE for details.
 */

#include "cache.h"
#include "git-compat-util.h"
#include "dump_export.h"
#include "dir_cache.h"

static struct strbuf props = STRBUF_INIT;
static unsigned long revn = 0;

/* Fills props and tells if the mode represents a file */
static int populate_props(mode_t mode)
{
	int is_file = 1;

	strbuf_reset(&props);
	switch (mode) {
	case S_IFINVALID:
		break;
	case 0644:
	case S_IFREG | 0644:
		break;
	case 0755:
	case S_IFREG | 0755:
		strbuf_addf(&props, "K 14\nsvn:executable\nV 1\n*\n");
		break;
	case S_IFLNK:
		strbuf_addf(&props, "K 11\nsvn:special\nV 1\n*\n");
		break;
	case S_IFGITLINK:
		die("Gitlinks unsupported"); /* TODO */
	case S_IFDIR:
		is_file = 0;
		break;
	default:
		die("Corrupt mode: %d", mode);
	}
	strbuf_add(&props, "PROPS-END\n", 10);
	return is_file;
}

void dump_export_revision(struct strbuf *revprops)
{
	printf("Revision-number: %lu\n", ++ revn);
	printf("Prop-content-length: %lu\n", revprops->len);
	printf("Content-length: %lu\n\n", revprops->len);
	printf("%s\n", revprops->buf);
}

static void dump_export_action(enum node_action action)
{
	switch (action) {
	case NODE_ACTION_CHANGE:
		printf("Node-action: change\n");
		break;
	case NODE_ACTION_ADD:
		printf("Node-action: add\n");
		break;
	case NODE_ACTION_DELETE:
		printf("Node-action: delete\n");
		break;
	case NODE_ACTION_REPLACE:
		printf("Node-action: replace\n");
		break;
	default:
		die("Corrupt action: %d", action);
	}
}

void dump_export_node(const char *path, mode_t mode,
		enum node_action action, size_t text_len,
		const char *copyfrom_path)
{
	/* Node-path, Node-kind, and Node-action */
	printf("Node-path: %s\n", path);
	printf("Node-kind: %s\n", populate_props(mode) ? "file" : "dir");
	dump_export_action(action);

	if (copyfrom_path) {
		printf("Node-copyfrom-rev: %lu\n", revn - 1);
		printf("Node-copyfrom-path: %s\n", copyfrom_path);
	}
	if (props.len) {
		printf("Prop-delta: false\n");
		printf("Prop-content-length: %lu\n", props.len);
	}
	if (text_len) {
		printf("Text-delta: false\n");
		printf("Text-content-length: %lu\n", text_len);
	}
	if (text_len || props.len) {
		printf("Content-length: %lu\n\n", text_len + props.len);
		printf("%s", props.buf);
	}

	/* When no data is present, pad with two newlines */
	if (!text_len)
		printf("\n\n");
}

void dump_export_mkdir(const char *path)
{
	dump_export_node(path, S_IFDIR, NODE_ACTION_ADD, 0, NULL);
}

void dump_export_m(const char *path, mode_t mode, size_t text_len)
{
	enum node_action action = NODE_ACTION_CHANGE;
	mode_t old_mode;

	old_mode = dir_cache_lookup(path);

	if (mode != old_mode) {
		if (old_mode != S_IFINVALID) {
			dump_export_d(path);
			dir_cache_remove(path);
		}
		action = NODE_ACTION_ADD;
		dir_cache_mkdir_p(path);
		dir_cache_add(path, mode);
	}

	dump_export_node(path, mode, action, text_len, NULL);
}

void dump_export_d(const char *path)
{
	printf("Node-path: %s\n", path);
	dump_export_action(NODE_ACTION_DELETE);
	printf("\n\n");
	dir_cache_remove(path);
}

void dump_export_cr(const char *path, const char *copyfrom_path,
		int delete_old)
{
	enum node_action action = NODE_ACTION_REPLACE;
	mode_t mode, old_mode;

	mode = dir_cache_lookup(path);
	old_mode = dir_cache_lookup(copyfrom_path);

	if (old_mode == S_IFINVALID)
		die("Can't copy from non-existant path: %s", copyfrom_path);
	if (mode != old_mode) {
		action = NODE_ACTION_ADD;
		dir_cache_mkdir_p(path);
		dir_cache_add(path, mode);
	}
	if (delete_old) {
		dump_export_d(copyfrom_path);
		dir_cache_remove(copyfrom_path);
	}

	dump_export_node(path, old_mode, action, 0, copyfrom_path);
}

void dump_export_init()
{
	printf("SVN-fs-dump-format-version: 3\n\n");
}
