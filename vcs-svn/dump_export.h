#ifndef DUMP_EXPORT_H_
#define DUMP_EXPORT_H_

#define MAX_GITSVN_LINE_LEN 4096
#define SVN_INVALID_REV 0

enum node_action {
	NODE_ACTION_CHANGE,
	NODE_ACTION_ADD,
	NODE_ACTION_DELETE,
	NODE_ACTION_REPLACE,
	NODE_ACTION_COUNT
};

void dump_export_revision(struct strbuf *revprops);
void dump_export_node(const char *path, mode_t mode,
		enum node_action action, size_t text_len,
		const char *copyfrom_path);
void dump_export_mkdir(const char *path);
void dump_export_m(const char *path, mode_t mode, size_t text_len);
void dump_export_d(const char *path);
void dump_export_cr(const char *path, const char *copyfrom_path,
		int delete_old);
void dump_export_init();

#endif
