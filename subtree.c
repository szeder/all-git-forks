/*
 * Utilities for identifying and creating subtree merges.
 *
 * (C) Copyright 2011 Nick Mayer
 *
 */

#include "builtin.h"
#include "subtree.h"
#include "string-list.h"
#include "tree-walk.h"

static int debug_printf_enabled = 0;
#define debug(...) if (debug_printf_enabled) fprintf(stderr, __VA_ARGS__)

struct subtree_details
{
    struct commit *commit;
    struct subtree_detail d;
};
       
static struct pathspec empty_pathspec;

/*-----------------------------------------------------------------------------
Parse the .subtree config file and put the paths (prefixes) for all subtrees
into the string_list pointed to by context.
-----------------------------------------------------------------------------*/
static int read_subtree_config(const char *var, const char *value, void *context)
{
    struct string_list *config = context;
    const char* substr = NULL;

    if ((substr = strstr(var, "subtree."))) {
        const char* dot;
        substr += 8; /* Skip past subtree. */
        if ((dot = strstr(substr, "."))) {
            dot += 1; /* Skip past the . */
            if( strstr( dot, "path" ) ) {
                struct strbuf tmp = STRBUF_INIT;
                strbuf_addstr(&tmp, value);
                string_list_append(config, tmp.buf);
                config->items[config->nr - 1].util = (void*) ((intptr_t) strlen(tmp.buf));
            }
        }
    }
    return 0;
}

/*-----------------------------------------------------------------------------
Store the SHA1 of the .subtree file in the strbuf pointed to by context
-----------------------------------------------------------------------------*/
static int get_subtree_sha1_read_tree(const unsigned char *sha1, const char *base, 
                                      int baselen, const char *pathname, 
                                      unsigned mode, int stage, void *context)
{
    int retval = 0;

    if (S_ISGITLINK(mode)) {
        return 0;
    } else if (S_ISDIR(mode)) {
        /* TODO: Support nested subtrees, or is that just a horrible idea? */
        //return READ_TREE_RECURSIVE;
        return 0;
    }

    if( strcmp( pathname, ".gitsubtree" ) == 0 ) {
        strbuf_add(context, (char *)sha1, 40);
        /* Found it, stop looking */
        return -1;
    }

    return retval;
}

/*-----------------------------------------------------------------------------
Find the tree sha1 values of the given prefixes and store them in pathinfo
-----------------------------------------------------------------------------*/
static int read_tree_find_subtrees(const unsigned char *sha1, const char *base,
                                   int baselen, const char *pathname,
                                   unsigned mode, int stage, void *context)
{
    int result = 0;
    int i;
    int pathlen;
    struct subtree_details *details = context;
    
    if (!S_ISDIR(mode)) {
        /* This isn't a folder, so we can't split off of it */
        return result;
    }

    pathlen = strlen(pathname);
    for (i = 0; i < details->d.nr; i++) {
        const char *prefix;
        int prefix_len;
      
        prefix = details->d.items[i].prefix;
        prefix_len = details->d.items[i].len;

        if (baselen > prefix_len)
            continue;
        if (strncmp(prefix, base, baselen) != 0)
            continue;

        if (strncmp(pathname, &prefix[baselen], pathlen) == 0) {
            if (baselen + pathlen == prefix_len)
                details->d.items[i].tree = lookup_tree(sha1);
            else
                result = READ_TREE_RECURSIVE;
        }
    }

    return result;
}

/*-----------------------------------------------------------------------------
Compare 2 trees, count added, removed, changed, and unchanged files
-----------------------------------------------------------------------------*/
enum tree_compare_result {
    TREE_SAME,
    TREE_MODIFIED,
    TREE_UNRELATED
};

struct compare_details {
    unsigned int add;
    unsigned int remove;
    unsigned int same;
    unsigned int change;
};

static enum tree_compare_result compare_trees(struct tree *tree1,
                                              struct tree *tree2,
                                              int recurse,
                                              struct compare_details *details)
{
    struct tree_desc t1, t2;
    int pathlen1;
    int pathlen2;
    int cmp;
    struct compare_details detail;
    memset(&detail, 0, sizeof(detail));

    /* Count up the number of matching tree objects between left & right */
    parse_tree(tree1);
    parse_tree(tree2);
    init_tree_desc(&t1, tree1->buffer, tree1->size);
    init_tree_desc(&t2, tree2->buffer, tree2->size);

    while (t1.size || t2.size) {
        pathlen1 = tree_entry_len(t1.entry.path, t1.entry.sha1);
        pathlen2 = tree_entry_len(t2.entry.path, t2.entry.sha1);
        if (t1.size == 0)
            cmp = -1;
        else if (t2.size == 0)
            cmp = 1;
        else
            cmp = base_name_compare(t1.entry.path, pathlen1, t1.entry.mode,
                                    t2.entry.path, pathlen2, t2.entry.mode);
        if (cmp < 0) {
            /* Removed from tree1 */
            detail.remove++;
        }
        else if (cmp > 0) {
            /* Added to tree 1 */
            detail.add++;
        }
        else if (t1.entry.mode == t2.entry.mode &&
                 hashcmp(t1.entry.sha1, t2.entry.sha1) == 0) {
            /* No changes */
            detail.same++;
        }
        else {
            /* Changed */
            if (recurse && S_ISDIR(t1.entry.mode) && S_ISDIR(t2.entry.mode)) {
                compare_trees(lookup_tree(t1.entry.sha1),
                              lookup_tree(t2.entry.sha1),
                              recurse, &detail);
            }
            else {
                detail.change++;
            }
        }

        update_tree_entry(&t1);
        update_tree_entry(&t2);
    }

    if (details) {
        details->add += detail.add;
        details->change += detail.change;
        details->remove += detail.remove;
        details->same += detail.same;
    }

    if (detail.same > 0) {
        if (detail.add + detail.change + detail.remove > 0)
            return TREE_MODIFIED;
        else
            return TREE_SAME;
    }
    else
        return TREE_UNRELATED;
}

/*-----------------------------------------------------------------------------
Find the subtree parent for this commit that matches the given tree
-----------------------------------------------------------------------------*/
struct commit *subtree_find_parent(struct commit *commit,
                                   struct tree *tree,
                                   int exact_match)
{
    struct commit_list *parent;
    struct commit *best_commit = NULL;

    /* Check our parents to see if this tree matches the tree node there */
    parent = commit->parents;
    while (parent)
    {
        struct commit *c = parent->item;
        if (c->tree == tree)
        {
            best_commit = c;
            break;
        }
        parent = parent->next;
    }

    /* 
     * If we allow non-exact matches, lets try a bit harder to see how close
     * we are by ranking each parent and picking the highest non-zero ranking
     * match.
     */
    if (!best_commit && !exact_match)
    {
        unsigned int best_val = 0;
        unsigned int alt_val = UINT_MAX;
        parent = commit->parents;
        while (parent)
        {
            struct compare_details matches;
            struct commit *c = parent->item;
            memset(&matches, 0, sizeof(matches));
            parse_commit(c);
            compare_trees(tree, c->tree, 1, &matches);
            if (matches.same > best_val) {
                best_val = matches.same;
                best_commit = c;
            }
            /* 
             * If we don't have anything with any matches, pick the thing that
             * has many similarly named files, if there aren't more adds/removes
             * than there are changes
             */
            if (best_val == 0
                && alt_val > (matches.add + matches.remove) 
                && matches.change > (matches.add + matches.remove)) {

                alt_val = (matches.add + matches.remove);
                best_commit = c;
            }
            parent = parent->next;
        }
    }

    return best_commit;
}


/*-----------------------------------------------------------------------------
Convert the given commit & prefix list to a subtree_details structure. 
The caller is responsible for freeing the memory.
-----------------------------------------------------------------------------*/
struct subtree_details *get_details(struct commit *commit, struct string_list *prefix_list)
{   
    struct subtree_details *details = NULL;
    struct string_list prefixes = STRING_LIST_INIT_NODUP;
    unsigned int i;

    /*
     * If we weren't given a list of subtree,s get the .subtree file's SHA,
     * and read in the list of subtree paths.
     */
    if (prefix_list != NULL && prefix_list->nr > 0) {
        for (i = 0; i < prefix_list->nr; i++) {
            string_list_append(&prefixes, prefix_list->items[i].string);
            prefixes.items[i].util = (void*) ((intptr_t) strlen(prefixes.items[i].string));
        }
    }
    else {
        struct strbuf subtree_blob_sha1 = STRBUF_INIT;

        parse_commit(commit);
        read_tree_recursive(commit->tree, "", 0, 0, &empty_pathspec, get_subtree_sha1_read_tree, &subtree_blob_sha1);
        if (subtree_blob_sha1.len > 0)
        {
            /* Read the .gitsubtree data for this commit */
            unsigned long size;
            enum object_type type;
            char *buf = read_sha1_file((unsigned char *)subtree_blob_sha1.buf, &type, &size);

            if (!buf) {
               error("Could not read object %s", sha1_to_hex((unsigned char *)subtree_blob_sha1.buf));
               return NULL;
            }

            die("Processing .gitsubtree not implemented");
            //git_config_from_buffer(read_subtree_config, buf, size, &prefixes);
            free(buf);
        }
        strbuf_release(&subtree_blob_sha1);
    }

    details = xmalloc(sizeof(*details) + sizeof(*(details->d.items)) * prefixes.nr);
    for (i = 0; i < prefixes.nr; i++) {
        const char* str = prefixes.items[i].string;
        details->d.items[i].prefix = str;
        details->d.items[i].len = strlen(str);
        details->d.items[i].tree = NULL;
    }
    details->d.nr = prefixes.nr;

    details->commit = commit;
    
    // TODO: Save this in details, and have a free_details function that cleans this all up?
    //string_list_clear(&prefixes, 0);

    return details;
}


/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
struct commit_list *get_subtrees(struct commit *commit, struct string_list *prefix_list, int exact)
{
    struct commit_list *subtree_commits = NULL;
    struct subtree_details *details = NULL;
    int i;

    debug("get_subtrees(%s)\n", sha1_to_hex(commit->object.sha1));

if (strcmp(sha1_to_hex(commit->object.sha1), "9636663fc8d39b0809070192ad81376d12e6b6c8")==0)
    i = 0;

    details = get_details(commit, prefix_list);

    read_tree_recursive(commit->tree, "", 0, 0, &empty_pathspec, read_tree_find_subtrees, details);
    
    for (i = 0; i < details->d.nr; i++) {
        debug("\t%s (%s)\n", details->d.items[i].prefix, details->d.items[i].tree ? sha1_to_hex(details->d.items[i].tree->object.sha1) : "none");

        if (details->d.items[i].tree) {
            struct commit *parent = NULL;
            /*
             * Check to see if one of this commit's parents is already the subtree
             * merge we're going to be generating
             */
            parent = subtree_find_parent(commit, details->d.items[i].tree, exact);
            if (parent) {
                commit_list_insert(parent, &subtree_commits);
                debug("\t\tFound subtree parent %s\n", sha1_to_hex(parent->object.sha1));
            }
        }
    }

    free(details);

    return subtree_commits;
}

/*-----------------------------------------------------------------------------
Returns a list of tree objects for the commit with the given prefix list.
TODO: Change to a tree_list
-----------------------------------------------------------------------------*/
struct subtree_detail *get_subtree_trees(struct commit *commit, struct string_list *prefix_list)
{
    int i;
    int sz;
    struct subtree_details *details = get_details(commit, prefix_list);
    struct subtree_detail *detail;

    /*
     * Read the tree to get the subtree's SHA1 (if it exists)
     */
    for (i = 0; i < details->d.nr; i++)
        details->d.items[i].tree = NULL;
    read_tree_recursive(commit->tree, "", 0, 0, &empty_pathspec, read_tree_find_subtrees, details);

    sz = sizeof(*detail) + sizeof(*(detail->items)) * details->d.nr;
    detail = xmalloc(sz);
    memcpy(detail, &details->d, sz);
    free(details);

    return detail;
}