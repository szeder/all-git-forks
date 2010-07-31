#ifndef __NARROW_TREE_H__
#define __NARROW_TREE_H__

struct pathspec;

extern void check_narrow_prefix(void);
extern struct pathspec *get_narrow_pathspec(void);
extern char *get_narrow_string(void);
extern int is_repository_narrow(void);

#endif
