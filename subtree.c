/*
 * Utilities for identifying and creating subtree merges.
 *
 * (C) Copyright 2011 Nick Mayer
 *
 */

#include "builtin.h"
#include "subtree.h"
#include "tree.h"
#include "tree-walk.h"
#include "revision.h"

static int debug_printf_enabled = 1;
#define debug(...) if (debug_printf_enabled) color_fprintf(stderr, GIT_COLOR_GREEN, __VA_ARGS__)
#define warn(...) if (debug_printf_enabled) color_fprintf(stderr, GIT_COLOR_RED, __VA_ARGS__)

/*-----------------------------------------------------------------------------
Find the tree sha1 values of the given prefixes and store them in pathinfo
-----------------------------------------------------------------------------*/
struct read_tree_find_subtrees_context
{
    struct string_list *prefix_list;
    struct tree **subtree_list;
};

static int read_tree_find_subtrees(const unsigned char *sha1, const char *base,
                                   int baselen, const char *pathname,
                                   unsigned mode, int stage, void *context)
{
    int result = 0;
    int i;
    int pathlen;
    struct read_tree_find_subtrees_context *details = context;
    
    if (!S_ISDIR(mode)) {
        /* This isn't a folder, so we can't split off of it */
        return result;
    }

    pathlen = strlen(pathname);
    for (i = 0; i < details->prefix_list->nr; i++) {
        const char *prefix;
        int prefix_len;
      
        prefix = details->prefix_list->items[i].string;
        prefix_len = strlen(prefix); /* TODO: Store this in the string list util */

        if (baselen > prefix_len)
            continue;
        if (strncmp(prefix, base, baselen) != 0)
            continue;

        if (strncmp(pathname, &prefix[baselen], pathlen) == 0) {
            if (baselen + pathlen == prefix_len)
                details->subtree_list[i] = lookup_tree(sha1);
            else
                result = READ_TREE_RECURSIVE;
        }
    }

    return result;
}

/*-----------------------------------------------------------------------------
For the commit, get the tree roots for the given prefixes (NULL if it 
doesn't exist)
-----------------------------------------------------------------------------*/
struct tree **subtree_trees(struct commit *commit, struct string_list *prefix_list) 
{
    static struct pathspec empty_pathspec = { 0 };
    struct read_tree_find_subtrees_context context;
    int sz;

    context.prefix_list = prefix_list;
    sz = sizeof(struct tree*) * prefix_list->nr;
    context.subtree_list = xmalloc(sz);
    memset(context.subtree_list, 0, sz);
    
    read_tree_recursive(commit->tree, "", 0, 0, &empty_pathspec, read_tree_find_subtrees, &context);

    return context.subtree_list;
}
