#ifndef SUBTREE_H
#define SUBTREE_H

#include "commit.h"
#include "string-list.h"

struct subtree_detail
{
    int nr;
    struct 
    {
	    const char *prefix;
	    size_t len;
        struct tree *tree;
    } items[];
};

/* TODO: Start all functions with subtree_ */
struct commit_list *get_subtrees(struct commit *commit, struct string_list *prefix_list, int exact);

struct subtree_detail *get_subtree_trees(struct commit *commit, struct string_list *prefix_list);

struct commit *subtree_find_parent(struct commit *commit,
                                   struct tree *tree,
                                   int exact_match);

#endif /* SUBTREE_H */