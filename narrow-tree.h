#ifndef __NARROW_TREE_H__
#define __NARROW_TREE_H__

struct index_state;
struct name_entry;
struct pathspec;
struct strbuf;

extern void check_narrow_prefix(void);
extern struct pathspec *get_narrow_pathspec(void);
extern char *get_narrow_string(void);
extern int is_repository_narrow(void);
extern int complete_cache_tree(struct index_state *istate);
extern int same_narrow_base(const unsigned char *t1, const unsigned char *t2);

#endif
