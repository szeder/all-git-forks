#ifndef ALLOC_H
#define ALLOC_H

extern void *alloc_blob_node(void);
extern void *alloc_tree_node(void);
extern void *alloc_commit_node(void);
extern void *alloc_tag_node(void);
extern void *alloc_object_node(void);
extern void alloc_report(void);
extern unsigned int alloc_commit_index(void);

#endif
