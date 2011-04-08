/*
 * Builtin "git subtree" and related commands
 *
 * (C) Copyright 2011 Nick Mayer
 *
 * Based on git-subtree.sh by Avery Pennarun and Jesse Greenwald's subtree 
 * patch for jgit.
 */

#include "builtin.h"
#include "cache-tree.h"
#include "dir.h"
#include "parse-options.h"
#include "refs.h"
#include "revision.h"
#include "run-command.h"
#include "tree-walk.h"
#include "unpack-trees.h"

/*---------------------------------------------------------------------------*/
/*              #####  ####### #     # #     # ####### #     #               */
/*             #     # #     # ##   ## ##   ## #     # ##    #               */
/*             #       #     # # # # # # # # # #     # # #   #               */
/*             #       #     # #  #  # #  #  # #     # #  #  #               */
/*             #       #     # #     # #     # #     # #   # #               */
/*             #     # #     # #     # #     # #     # #    ##               */
/*              #####  ####### #     # #     # ####### #     #               */
/*---------------------------------------------------------------------------*/

static int opt_string_list(const struct option *opt, const char *arg, int unset)
{
    struct string_list *exclude_list = opt->value;
    string_list_append(exclude_list, arg);
    return 0;
}

static int debug_printf_enabled = 0;
#define debug(...) if( debug_printf_enabled ) printf(__VA_ARGS__)

/*---------------------------------------------------------------------------*/
/*                             #    ######  ######                           */
/*                            # #   #     # #     #                          */
/*                           #   #  #     # #     #                          */
/*                          #     # #     # #     #                          */
/*                          ####### #     # #     #                          */
/*                          #     # #     # #     #                          */
/*                          #     # ######  ######                           */
/*---------------------------------------------------------------------------*/

struct add_opts {
    int no_dot_subtree;
    int squash;
    const char* repo;
    const char* name;
    const char* prefix;
};

static const char * const builtin_subtree_add_usage[] = {
    "git subtree add [options] branch",
    NULL
};

static int cmd_subtree_add(int argc, const char **argv, const char *prefix)
{
    struct add_opts opts;
    int newfd;
    unsigned int i;
    struct lock_file lock_file;
    struct tree *tree;
    struct tree_desc tree_desc;
    struct unpack_trees_options unpack_opts;
    unsigned char merge_sha1[20]; 
    unsigned char result_tree[20];
    unsigned char result_commit[20];
    struct commit_list* parents = NULL;
    struct strbuf commit_msg = STRBUF_INIT;
    struct checkout state;
    static char topath[PATH_MAX + 1];
    const char *pathspec[2];
    const char *branch_name;
    struct option options[] = {
        OPT_STRING('r', "remote", &opts.repo, "repo", "Location of external repository to fetch branch from"),
        OPT_BOOLEAN(0, "squash", &opts.squash, "Bring history in as one commit"), /* TODO */
        OPT_STRING('n', "name", &opts.name, "subtree name", "Name of the subtree"), /* TODO: Add .subtree or remove name option */
        OPT_STRING('P', "prefix", &opts.prefix, "prefix", "Location to add subtree"),
        OPT_END(),
        };

    /* Parse arguments */
    memset(&opts, 0, sizeof(opts));
    argc = parse_options(argc, argv, prefix, options, builtin_subtree_add_usage, PARSE_OPT_KEEP_DASHDASH);

    if (opts.prefix == NULL) {
        die("git subtree add: must specify prefix");
    }

    /* TODO: Verify this prefix doesn't already exist in the tree (or locally)? */
    if (argc != 1) {
        die("git subtree add: branch must be specified");
    }

    newfd = hold_locked_index(&lock_file, 1);

    if (read_cache_unmerged())
        die("You need to resolve your current index first");

    if (opts.repo) {
        char *fetch_argv[10];
        int fetch_result;
        int i = 0;
        fetch_argv[i++] = "fetch";
        fetch_argv[i++] = opts.repo;
        fetch_argv[i++] = argv[0];
        fetch_argv[i++] = "--quiet";
        fetch_argv[i] = NULL;
        fetch_result = cmd_fetch(i, fetch_argv, "");
        if (fetch_result)
            return fetch_result;

        branch_name = "FETCH_HEAD";
    }
    else {
        branch_name = argv[0];
    }

    /* TODO: Add option to fetch from a remote first, then use FETCH_HEAD to get sha1. */
    if (get_sha1(branch_name, merge_sha1))
        die("git subtree add: Valid branch must be specified");

    debug("Add commit %s\n", sha1_to_hex(merge_sha1));

    tree = parse_tree_indirect(merge_sha1);
    init_tree_desc(&tree_desc, tree->buffer, tree->size);

    git_config(git_default_config, NULL);
    setup_work_tree();

    memset(&unpack_opts, 0, sizeof(unpack_opts));
    unpack_opts.head_idx = 1;
    unpack_opts.src_index = &the_index;
    unpack_opts.dst_index = &the_index;
    unpack_opts.prefix = opts.prefix;
    unpack_opts.merge = 1;
    unpack_opts.update = 1;
    unpack_opts.fn = bind_merge;

	cache_tree_free(&active_cache_tree);
    if (unpack_trees(1, &tree_desc, &unpack_opts)) {
        rollback_lock_file(&lock_file);
        die("Unable to read tree");
    }

    /* checkout */
    strcpy(topath, prefix);
    memset(&state, 0, sizeof(state));
    state.base_dir = "";
    state.force = 1;
    state.refresh_cache = 1;
    pathspec[0] = opts.prefix;
    pathspec[1] = NULL;
	for (i = 0; i < the_index.cache_nr; i++)  {
        struct cache_entry *ce = the_index.cache[i];
		if (match_pathspec(pathspec, ce->name, ce_namelen(ce), 0, NULL)) {
			if (!ce_stage(ce)) {
				checkout_entry(ce, &state, NULL);
				continue;
			}
		}
    }

    if (write_cache(newfd, active_cache, active_nr) || commit_locked_index(&lock_file))
        die("unable to write new index file");
    
    /* At this point things are staged & in the index, but not committed */
    parents = NULL;
    if (opts.squash) {
        struct commit *commit;
        unsigned char result_commit[20];
        struct strbuf commit_msg = STRBUF_INIT;
        strbuf_addstr(&commit_msg, "Subtree add squash ");
        strbuf_addstr(&commit_msg, argv[0]);

        commit = lookup_commit(merge_sha1);
        parse_commit(commit);
        commit_tree
            (
            commit_msg.buf,
            commit->tree->object.sha1, 
            NULL, 
            result_commit, 
            NULL, 
            NULL
            );
        strbuf_release(&commit_msg);

        commit = lookup_commit(result_commit);
        commit_list_insert(commit, &parents);
    }
    else {
        commit_list_insert(lookup_commit(merge_sha1), &parents);
    }
    commit_list_insert(lookup_commit_reference_by_name("HEAD"), &parents);
    
	if (write_cache_as_tree(result_tree, 0, NULL))
		die("git write-tree failed to write a tree");

    strbuf_addstr(&commit_msg, "Subtree add ");
    strbuf_addstr(&commit_msg, argv[0]);
    if (opts.repo) {
        strbuf_addstr(&commit_msg, " on ");
        strbuf_addstr(&commit_msg, opts.repo);
    }
    strbuf_addstr(&commit_msg, " into ");
    strbuf_addstr(&commit_msg, opts.prefix);

	commit_tree(commit_msg.buf, result_tree, parents, result_commit, NULL, NULL);
    strbuf_release(&commit_msg);

    printf("%s\n", sha1_to_hex(result_commit));

    /* Now we just need to move the current tree up to the newly created commit */
    update_ref("subtree add", "HEAD", result_commit, NULL, 0, DIE_ON_ERR);
    return 0;
}
 
/*---------------------------------------------------------------------------*/
/*                        #       ###  #####  #######                        */
/*                        #        #  #     #    #                           */
/*                        #        #  #          #                           */
/*                        #        #   #####     #                           */
/*                        #        #        #    #                           */
/*                        #        #  #     #    #                           */
/*                        ####### ###  #####     #                           */
/*---------------------------------------------------------------------------*/

struct list_opts {
    int exact;
    struct string_list prefix_list;
};

static const char * const builtin_subtree_list_usage[] = {
    "git subtree list [options] <paths>",
    "git subtree list [options] <branch> <paths>",
    "git subtree list [options] [<branch>] -- <paths>",
    NULL,
};

static int cmd_subtree_list(int argc, const char **argv, const char *prefix)
{
    struct list_opts opts;
    struct option options[] = {
        OPT_BOOLEAN(0, "exact", &opts.exact, "Only list exact subtree matches"),
        OPT_CALLBACK('P', "prefix", &opts.prefix_list, "prefix", "prefix <path>", opt_string_list),
        OPT_END(),
        };

    /* Parse arguments */
    memset(&opts, 0, sizeof(opts));
    argc = parse_options(argc, argv, prefix, options, builtin_subtree_list_usage, PARSE_OPT_KEEP_DASHDASH);

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                  #     # ####### ######   #####  #######                  */
/*                  ##   ## #       #     # #     # #                        */
/*                  # # # # #       #     # #       #                        */
/*                  #  #  # #####   ######  #  #### #####                    */
/*                  #     # #       #   #   #     # #                        */
/*                  #     # #       #    #  #     # #                        */
/*                  #     # ####### #     #  #####  #######                  */
/*---------------------------------------------------------------------------*/

static const char * const builtin_subtree_merge_usage[] = {
	"git subtree merge -P <commit>",
	NULL
};

struct merge_opts {
    int squash;
    const char* prefix;
};

static int cmd_subtree_merge(int argc, const char **argv, const char *prefix)
{
	int retval;
    int i, j;
    struct merge_opts opts;
    struct strbuf subtree_strategy = STRBUF_INIT;
    const char **subtree_argv;
	struct option options[] = {
        OPT_BOOLEAN(0, "squash", &opts.squash, "Bring history in as one commit"), /* TODO */
        OPT_STRING('P', "prefix", &opts.prefix, "prefix", "Location to add subtree"),
        //OPT_STRING('r', "remote", &opts.repo, "repo", "Location of external repository to merge branch from"), /* TODO */
		OPT_END()
	};

    memset(&opts, 0, sizeof(opts));
	argc = parse_options(argc, argv, prefix, options, builtin_subtree_merge_usage, 0);

	if (!opts.prefix) 
    {
        /* TODO: Determine prefix from current directory? */
		error("Must specify a prefix");
		usage_with_options(builtin_subtree_merge_usage, options);
	}


    subtree_argv = xmalloc(sizeof(char**) * (argc + 5));
    i = 0;
    subtree_argv[i++] = "merge";
    strbuf_addstr(&subtree_strategy, "-Xsubtree=");
    strbuf_addstr(&subtree_strategy, opts.prefix);
    subtree_argv[i++] = subtree_strategy.buf;
    for (j = 0; j < argc; j++)
    {
        subtree_argv[i+j] = argv[j];
    }
    subtree_argv[i+j] = NULL;

    /* Call into merge to actually do the work */
    retval = cmd_merge(i+j, subtree_argv, prefix);
    
    strbuf_release(&subtree_strategy);
	return retval;
}

/*---------------------------------------------------------------------------*/
/*                      ######  #     # #       #                            */
/*                      #     # #     # #       #                            */
/*                      #     # #     # #       #                            */
/*                      ######  #     # #       #                            */
/*                      #       #     # #       #                            */
/*                      #       #     # #       #                            */
/*                      #        #####  ####### #######                      */
/*---------------------------------------------------------------------------*/

static const char * const builtin_subtree_pull_usage[] = {
    "git subtree pull [options] [<repository> [<refspec>]]",
    NULL,
};

struct pull_opts {
    const char* prefix;
};

static int cmd_subtree_pull(int argc, const char **argv, const char *prefix)
{
	int retval;
    int i, j;
    struct pull_opts opts;
    struct strbuf subtree_strategy = STRBUF_INIT;
    const char **subtree_argv;
	struct option options[] = {
        OPT_STRING('P', "prefix", &opts.prefix, "prefix", "Location to add subtree"),
		OPT_END()
	};

    memset(&opts, 0, sizeof(opts));
	argc = parse_options(argc, argv, prefix, options, builtin_subtree_merge_usage, 0);

	if (!opts.prefix) 
    {
        /* TODO: Determine prefix from current directory? */
		error("Must specify a prefix");
		usage_with_options(builtin_subtree_merge_usage, options);
	}

    strbuf_addstr(&subtree_strategy, "-Xsubtree=");
    strbuf_addstr(&subtree_strategy, opts.prefix);

    subtree_argv = xmalloc(sizeof(char**) * (argc + 5));
    i = 0;
    subtree_argv[i++] = "pull";
    subtree_argv[i++] = subtree_strategy.buf;
    for (j = 0; j < argc; j++)
    {
        subtree_argv[i+j] = argv[j];
    }
    subtree_argv[i+j] = NULL;

    /* Call into pull to actually do the work */
    retval = run_command_v_opt(subtree_argv, RUN_GIT_CMD);
	
    strbuf_release(&subtree_strategy);
	return retval;
}

/*---------------------------------------------------------------------------*/
/*                      ######  #     #  #####  #     #                      */
/*                      #     # #     # #     # #     #                      */
/*                      #     # #     # #       #     #                      */
/*                      ######  #     #  #####  #######                      */
/*                      #       #     #       # #     #                      */
/*                      #       #     # #     # #     #                      */
/*                      #        #####   #####  #     #                      */
/*---------------------------------------------------------------------------*/
                      
static const char * const builtin_subtree_push_usage[] = {
    "git subtree push",
    NULL,
};

static int cmd_subtree_push(int argc, const char **argv, const char *prefix)
{
    /* TODO */
    return 0;
}        

/*---------------------------------------------------------------------------*/
/*                     #####  ######  #       ### #######                    */
/*                    #     # #     # #        #     #                       */
/*                    #       #     # #        #     #                       */
/*                     #####  ######  #        #     #                       */
/*                          # #       #        #     #                       */
/*                    #     # #       #        #     #                       */
/*                     #####  #       ####### ###    #                       */
/*---------------------------------------------------------------------------*/

struct split_opts {
    int rewrite_parents;
    int change_committer;
    int always_create;
    int rejoin;
    int squash;
    const char *annotation;
    const char *footer;
    struct string_list onto_list;
    struct string_list prefix_list;
    const char *output;             /* TODO: Output format to show all commits, just head(s), commits by prefix, etc */
};

static struct path_info {
    struct tree* tree;
} *pathinfo;

static struct string_list *prefix_list;
static struct commit_list *onto_list;

struct commit_util {
    struct commit *remapping;
    struct tree *tree;
    int ignore : 1;   
    /* Did we create this commit */
    int created : 1;
    /* Is this commit on the subtree (meaning remapping points to original commit) */
    int is_subtree : 1;
};

__inline struct commit_util *get_commit_util(struct commit *commit, 
                                             int index, 
                                             int create) /* =2 if assert on non-existence */
{
    if(!commit->util) {
        int sz;
        assert(create != 2);
        if (!create)
            return NULL;

        /* Allocate one for each prefix, and an extra for self */
        sz = sizeof(struct commit_util) * ( prefix_list->nr + 1);
        /* TODO: Find a way to eventually free this memory? */
        commit->util = malloc(sz);
        memset(commit->util, 0, sz);
    }
    return &((struct commit_util*)commit->util)[index];
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
    struct commit *commit = context;

    if (!S_ISDIR(mode)) {
        /* This isn't a folder, so we can't split off of it */
        return result;
    }

    pathlen = strlen(pathname);
    for (i = 0; i < prefix_list->nr; i++) {
        char *prefix;
        int prefix_len;
        struct commit_util *util;

        /* 
         * Don't bother with ignored subtrees other than to propagate the 
         * ignore
         */
        util = get_commit_util(commit, i, 0);
        if (util && util->ignore) {
            struct commit_list *parent = commit->parents;
            while (parent) {
                get_commit_util( parent->item, i, 1)->ignore = 1;
                parent = parent->next;
            }
            continue;
        }

        prefix = prefix_list->items[i].string;
        prefix_len = (int)((intptr_t) prefix_list->items[i].util);
        
        if (baselen > prefix_len) 
            continue;
        if (strncmp(prefix, base, baselen) != 0)
            continue;

        if (strncmp(pathname, &prefix[baselen], pathlen) == 0) {
            if (baselen + pathlen == prefix_len)
                pathinfo[i].tree = lookup_tree(sha1);
            else
                result = READ_TREE_RECURSIVE;
        }
    }

    return result;
}

/*-----------------------------------------------------------------------------
compare_trees
Compare 2 trees, count added, removed, changed, and unchanged files
-----------------------------------------------------------------------------*/
enum tree_compare_result {
    TREE_SAME,
    TREE_MODIFIED,
    TREE_DIFFERENT
};

struct compare_details {
    int add;
    int remove;
    int same;
    int change;
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
        return TREE_DIFFERENT;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static struct commit *find_subtree_parent(struct commit *commit, 
                                          struct tree *tree, 
                                          int exact_match)
{
    struct commit_list *parent;

    /* Check our parents to see if this tree matches the tree node there */
    parent = commit->parents;
    while (parent)
    {
        struct commit *c = parent->item;
        if (c->tree == tree)
        {
            return c;
        }
        parent = parent->next;
    }

    /* If we allow non-exact matches, lets try a bit harder to see how close
        we are by ranking each parent and picking the highest non-zero ranking
        match */
    if (!exact_match)
    {
        int best_val = 0;
        struct commit *best_commit = NULL;
        parent = commit->parents;
        while (parent)
        {
            struct compare_details matches;
            struct commit *c = parent->item;
            memset(&matches, 0, sizeof(matches));
            compare_trees(tree, c->tree, 1, &matches);
            if (matches.same > best_val)
            {
                best_val = matches.same;
                best_commit = c;
            }
            parent = parent->next;
        }

        return best_commit;
    }

    return NULL;
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/
static struct commit *rewrite_commit(struct commit *commit,
                                     struct tree *tree,
                                     struct commit_list *remapped_parents,
                                     int is_subtree,
                                     struct split_opts *opts)
{
    const char* body;
    const char* author;
    const char* committer;
    int len;
    struct strbuf body_str = STRBUF_INIT;
    struct strbuf author_str = STRBUF_INIT;
    struct strbuf committer_str = STRBUF_INIT;
    unsigned char output_commit_sha1[20];
    
    /* The commit buffer contains tree-id, parents, etc. */
    len = find_commit_subject(commit->buffer, &body);
    len = find_commit_author(commit->buffer, &author);
    strbuf_add(&author_str, author, len);
    author = author_str.buf;

    /* 
     * TODO: Take a param so they can optionally specify committer info
     * instead of reading from environment? 
     */
    if (!opts->change_committer)
    {
        len = find_commit_committer(commit->buffer, &committer);
        strbuf_add(&committer_str, committer, len);
        committer = committer_str.buf;
    }
    else
    {
        committer = NULL;
    }

    if (is_subtree && opts->annotation)
    {
        strbuf_addstr(&body_str, opts->annotation);
    }
    strbuf_addstr(&body_str, body);
    if (is_subtree && opts->footer)
    {
        strbuf_addstr(&body_str, opts->footer);
    }
    body = body_str.buf;

    /* Create a new commit object */
    hashclr(output_commit_sha1);
    commit_tree(body, tree->object.sha1, remapped_parents, output_commit_sha1, author, committer);
    strbuf_release(&author_str);
    strbuf_release(&committer_str);
    strbuf_release(&body_str);
    /* commit_tree frees remapped_parents for us */

    return lookup_commit( output_commit_sha1 );
}

/*-----------------------------------------------------------------------------
-----------------------------------------------------------------------------*/

static const char * const builtin_subtree_split_usage[] = {
    "git subtree split [options] <paths>",
    "git subtree split [options] <branch> <paths>",
    "git subtree split [options] [<branch>] -- <paths>",
    NULL,
};

/*-----------------------------------------------------------------------------
TODO: Could split get confused when using 'onto' if the specified onto tree is in
the history multiple times (due to a undo commit)
-----------------------------------------------------------------------------*/
static int cmd_subtree_split(int argc, const char **argv, const char *prefix)
{
    struct split_opts opts;
    struct option options[] = {
        OPT_BOOLEAN(0, "rewrite-parents", &opts.rewrite_parents, "Rewrite the commits that are split from to include the generated commit as a subtree merge"), /* TODO: Take an argument as to which commits to rewrite */
        OPT_BOOLEAN(0, "rejoin", &opts.rejoin, "Add a merge commit that joins the split out subtree with the source"), 
        OPT_BOOLEAN(0, "squash", &opts.squash, "Don't bring in the entire split history"), 
        OPT_BOOLEAN(0, "committer", &opts.change_committer, "Rewritten commits will use current commiter information"),
        OPT_BOOLEAN(0, "always-create", &opts.always_create, "Even if a child with the proper split tree-id already exists, we'll still recreate it"),
        OPT_CALLBACK(0, "onto", &opts.onto_list, "commit", "Graft the split subtree onto the given commit", opt_string_list),
        OPT_STRING(0, "annotate", &opts.annotation, "annotation", "Add an annotation to the beginning of the commit message of split commits"),
        OPT_STRING(0, "footer", &opts.footer, "annotation", "Add an annotation to the end of the commit message of split commits"),
        OPT_CALLBACK('P', "prefix", &opts.prefix_list, "prefix", "prefix <path>", opt_string_list),
        OPT_END(),
    };
    int i;
    int pathinfo_sz;
    struct rev_info rev;
    struct setup_revision_opt opt;  
    struct commit *commit;
    struct commit *rewritten_commit;
    struct commit_list *interesting_commits = NULL;
    struct commit_list **rewritten_commits = NULL;
    int sz;
    int cnt;
    int has_subtree_data;
    struct commit* head;

    /* Parse arguments */
    memset(&opts, 0, sizeof(opts));
    argc = parse_options(argc, argv, prefix, options, builtin_subtree_split_usage, PARSE_OPT_KEEP_UNKNOWN);

    if (opts.squash || opts.rejoin) {
        if (opts.rewrite_parents )
            die("git subtree split: Can't rewrite parents and do a squash or a join");
        
        head = lookup_commit_reference_by_name("HEAD");
        parse_commit(head);
    }

    /*
     * Populate the util with the string length so we're not constantly 
     * recomputing and allocate memory for the returned tree SHA1s 
     */
    prefix_list = &opts.prefix_list;
    for (i = 0; i < prefix_list->nr; i++) {
        int len = strlen(opts.prefix_list.items[i].string);
        if (opts.prefix_list.items[i].string[len-1] == '/')
        {
            len--;
            opts.prefix_list.items[i].string[len] = '\0';
        }
        opts.prefix_list.items[i].util = (void*) ((intptr_t) len);
    }
    pathinfo_sz = sizeof(*pathinfo) * prefix_list->nr;
    pathinfo = xmalloc(pathinfo_sz);

    /*
     * Setup the onto list
     */
    onto_list = NULL;
    for(i = 0; i < opts.onto_list.nr; i++)
    {
        int j;
        commit = lookup_commit_reference_by_name(opts.onto_list.items[i].string);
        if (!commit)
            die("git subtree split: unable to resolve onto %s", opts.onto_list.items[i].string);

        parse_commit(commit);
        commit_list_insert(commit, &onto_list);

        /* 
         * We don't need to process any of these commits 
        */
        for( j = 0; j <= prefix_list->nr; j++)
            get_commit_util(commit, j, 1)->ignore = 1;
    }

    /* 
     * Setup the walk. Make sure the user didn't pass any flags that will 
     * mess things up
     */
    init_revisions(&rev, prefix);
    rev.topo_order = 1;
    rev.reverse = 0;
    rev.bisect = 0;
    rev.ignore_merges = 0;
    rev.max_parents = -1;
    rev.min_parents = 0;

    memset(&opt, 0, sizeof(opt));
    opt.def = "HEAD";

    /* HACK: argv[0] is ignored by setup_revisions */
    argc = setup_revisions(argc+1, argv-1, &rev, &opt) - 1;
    if (argc)
        die("Unknown option: %s", argv[0]);

    prepare_revision_walk(&rev);
    while ((commit = get_revision(&rev))) {
        debug("%s processing...\n", sha1_to_hex(commit->object.sha1));
        
        memset(pathinfo, 0, pathinfo_sz);
        read_tree_recursive(commit->tree, "", 0, 0, NULL, read_tree_find_subtrees, commit);
        has_subtree_data = 0;
        for (i = 0; i < prefix_list->nr; i++) {
            if (pathinfo[i].tree) {
                struct commit_util *util;
                struct commit_list *onto;

                /* 
                 * If the tree id matches one of the onto trees, we don't need
                 * to search any further
                 */
                onto = onto_list;
                while (onto) {
                    struct commit_list *parents;
                    if (pathinfo[i].tree == onto->item->tree) {
                        
                        debug("\tFound onto %s for %s\n", 
                            sha1_to_hex(onto->item->object.sha1), 
                            opts.prefix_list.items[i].string);

                        util = get_commit_util(commit, i, 1);
                        util->remapping = onto->item;
                        util->tree = onto->item->tree;
                        util->ignore = 0;
                        
                        parents = commit->parents;
	                    while (parents) {
                            util = get_commit_util(parents->item, i, 1);
                            util->ignore = 1;
		                    parents = parents->next;
	                    }
                        /* TODO: Remove from the onto list */
                        break;
                    }
                    onto = onto->next;
                }
                if (onto)
                    continue;

                /* 
                 * Check to see if one of this commit's parents is already the subtree
                 * merge we're going to be generating 
                 */
                if (!opts.always_create) {
                    struct commit *parent = find_subtree_parent(commit, pathinfo[i].tree, 0);
                    if (parent) {
                        struct commit_list *parents;

                        debug("\tFound existing parent %s for %s\n", 
                            sha1_to_hex(parent->object.sha1), 
                            opts.prefix_list.items[i].string);

                        /*
                         * We've found an existing subtree commit. Set the 
                         * remap info so we know not to try to create a new
                         * commit in the next stage
                         */
                        util = get_commit_util(commit, i, 1);
                        util->remapping = parent;
                        util->tree = parent->tree;
                        util->ignore = 0;

                        /*
                         * We'll propagate this as we see subtree commits to 
                         * save on setting this on things we may never even
                         * go far enough to see.
                         * TODO: How will this work with --all passed in as a 
                         * refspec and a feature branch that removed the subtree?
                         * Setting a flag on the parent saying it needs to 
                         * process that can override the don't process flag 
                         * would work I guess..
                         */
                        parents = commit->parents;
	                    while (parents) {
                            util = get_commit_util(parents->item, i, 1);
                            util->ignore = 1;
		                    parents = parents->next;
	                    }
                        continue;
                    }
                }

                util = get_commit_util(commit, i, 1);
                util->ignore = 0;
                util->tree = pathinfo[i].tree;

                debug("\tFound tree %s for %s\n", 
                    sha1_to_hex(pathinfo[i].tree->object.sha1), 
                    opts.prefix_list.items[i].string);

                has_subtree_data = 1;
            }
        }

        /* Add this commit to the list for processing (in reverse order) */
        if (has_subtree_data)
            commit_list_insert(commit, &interesting_commits);
    }

    debug("\n\n");

    sz = sizeof(*rewritten_commits) * (prefix_list->nr + 1);
    rewritten_commits = malloc(sz);
    memset(rewritten_commits, 0, sz);

    /*
     * Now that we've collected all of the relevant commits, we'll go through
     * and generate subtree commits for them as needed.
     */
    while ((commit = pop_commit(&interesting_commits)) != NULL) {
        debug("%s Creating subtree...\n", sha1_to_hex(commit->object.sha1));

        /*
         * Generate the split out commits
         */
        for (i = 0; i < prefix_list->nr; i++) {
            struct commit_util *commit_util;
            struct commit_list *p;
            struct commit_list *parent;
            struct commit_util *parent_util;

            debug("\t%s\n", prefix_list->items[i].string);

            commit_util = get_commit_util(commit, i, 2);
            if (commit_util->ignore || !commit_util->tree)
                continue;
            if (commit_util->remapping) {
                debug("\t\tAlready split\n");
                continue;
            }

            /*
             * Check this commit's parents and see if the tree id has changed
             */
            p = commit->parents;
            while (p) {
                struct commit_list *remapped_parents = NULL;
                struct commit_util *tmp_util;
                struct commit_list *tmp_parent;
                struct commit_list **insert;

                parent = p;
                p = p->next;

                parent_util = get_commit_util(parent->item, i, 0);
                debug("\t\tChecking parent %s\n", sha1_to_hex(parent->item->object.sha1));
                if (!parent_util) {
                }
                else
                {
                    if (parent_util->remapping) {
                        parse_commit(parent_util->remapping);
                        if (parent_util->remapping->tree == commit_util->tree) {
                            /* 
                             * The tree hasn't changed from one of our parents
                             * so we don't need to merge it in.
                             */
                            commit_util->remapping = parent_util->remapping;
                            debug("\t\tFOUND\n");
                            break;;
                        }
                    }
                    if (parent_util->ignore) {
                        debug("\t\tIGNORE\n");
                        continue;
                    }
                }

                /* 
                 * Map the existing parents to their new values 
                 */
                insert = &remapped_parents;
                tmp_parent = commit->parents;
                while (tmp_parent) {
                    tmp_util = get_commit_util(tmp_parent->item, i, 0);
                    if (tmp_util && tmp_util->remapping) {
                        insert = &commit_list_insert(tmp_util->remapping, insert)->next;
                        /*
                         * Mark the remapped commit as ignored so we know it
                         * has parents and doesn't need to be displayed.
                         */
                        tmp_util = get_commit_util(tmp_util->remapping, i, 1);
                        tmp_util->ignore = 1;
                    }
                    tmp_parent = tmp_parent->next;
                }

                rewritten_commit = rewrite_commit(commit, commit_util->tree, remapped_parents, 1, &opts);
                commit_util->remapping = rewritten_commit;
                commit_util->created = 1;

                tmp_util = get_commit_util(rewritten_commit, i, 1);
                tmp_util->remapping = commit;
                tmp_util->created = 1;
                tmp_util->is_subtree = 1;

                debug("\t\t*** CREATED %s\n", sha1_to_hex(rewritten_commit->object.sha1));
                commit_list_insert(commit, &rewritten_commits[i]);
            }
        }
        
        /* Rewrite the parent (if requested) */
        if (opts.rewrite_parents) {
            struct commit_util *commit_util;
            struct commit_list *remapped_parents = NULL;
            struct commit_util *tmp_util;
            struct commit_list *tmp_parent;
            struct commit_list **insert;
            int is_changed;

            /* 
             * Map the existing parents to their new values 
             */
            is_changed = 0;
            insert = &remapped_parents;
            tmp_parent = commit->parents;
            while (tmp_parent) {
                tmp_util = get_commit_util(tmp_parent->item, prefix_list->nr, 0);
                if (tmp_util && tmp_util->remapping) {
                    insert = &commit_list_insert(tmp_util->remapping, insert)->next;
                    is_changed = 1;
                    /*
                     * Mark the remapped commit as ignored so we know it
                     * has parents and doesn't need to be displayed.
                     */
                    tmp_util = get_commit_util(tmp_util->remapping, i, 1);
                    tmp_util->ignore = 1;
                } 
                else {
                    insert = &commit_list_insert(tmp_parent->item, insert)->next;
                }
                tmp_parent = tmp_parent->next;
            }

            /*
             * Now add any created subtrees
             */
            for (i = 0; i < prefix_list->nr; i++) {
                tmp_util = get_commit_util(commit, i, 2);
                if (tmp_util->created) {
                    insert = &commit_list_insert(tmp_util->remapping, insert)->next;
                    is_changed = 1;
                }
            }

            if (is_changed) {
                commit_util = get_commit_util(commit, prefix_list->nr, 2);
                rewritten_commit = rewrite_commit(commit, commit->tree, remapped_parents, 0, &opts);
                commit_util->remapping = rewritten_commit;
                debug("\t*** REWRITE %s\n", sha1_to_hex(rewritten_commit->object.sha1));
                commit_list_insert(commit, &rewritten_commits[prefix_list->nr]);
            }
            else {
                free_commit_list(remapped_parents);
            }
        }
    }

    interesting_commits = NULL;
    cnt = prefix_list->nr;
    if (opts.rewrite_parents)
        cnt++;
    for (i = 0; i < cnt; i++) {
        struct commit_list *rewritten;
        struct commit_list *squash_commits = NULL;
        printf("%s\n", i < prefix_list->nr ? prefix_list->items[i].string : "HEAD");
        
        rewritten = rewritten_commits[i];
        while (rewritten) {
            struct commit_util *util_rewrite;
            struct commit_util *util_remap;

            util_rewrite = get_commit_util(rewritten->item, i, 2);
 
            if (opts.squash) {
                /* 
                 * TODO: This could be optimized quite a bit above, but this is
                 * quick and easy to implement
                 */
                struct commit_list *parents = util_rewrite->remapping->parents;
                while (parents)
                {
                    util_remap = get_commit_util(parents->item, i, 0);
                    if (!util_remap || !util_remap->created) {
                        debug("\tSquash %s to %s\n", sha1_to_hex(util_rewrite->remapping->object.sha1), sha1_to_hex(parents->item->object.sha1));
                        commit_list_insert(parents->item, &squash_commits);
                    }
                    parents = parents->next;
                }

                /* TODO: Optionally take all of the commit messages from these and build them into one? */
            }
            else {
                util_remap = get_commit_util(util_rewrite->remapping, i, 0);
                if (!util_remap || !util_remap->ignore) {
                    printf("\t%s %s\n", sha1_to_hex(util_rewrite->remapping->object.sha1), sha1_to_hex(rewritten->item->object.sha1));
                    commit_list_insert(util_rewrite->remapping, &interesting_commits);
                }
            }

            rewritten = rewritten->next;
        }

        if (opts.squash) {
            unsigned char result_commit[20];
            struct commit_util *util;
            struct strbuf commit_msg = STRBUF_INIT;
            strbuf_addstr(&commit_msg, "Subtree split squash ");
            strbuf_addstr(&commit_msg, prefix_list->items[i].string);

            util = get_commit_util(head, i, 0);
	        commit_tree
                (
                commit_msg.buf,
                util->tree->object.sha1, 
                squash_commits, 
                result_commit, 
                NULL, 
                NULL
                );
            strbuf_release(&commit_msg);

            commit = lookup_commit(result_commit);
            commit_list_insert(commit, &interesting_commits);
            printf("\t%s %s\n", sha1_to_hex(commit->object.sha1), sha1_to_hex(head->object.sha1));
        }

        free_commit_list(rewritten_commits[i]);
    }
    free(rewritten_commits);
    free(pathinfo);

    if (opts.rejoin) {
        unsigned char result_commit[20];
        struct strbuf commit_msg = STRBUF_INIT;
        strbuf_addstr(&commit_msg, "Subtree split rejoin\n\n");
        for (i = 0; i < prefix_list->nr; i++) {
            strbuf_addstr(&commit_msg, prefix_list->items[i].string);
            strbuf_addstr(&commit_msg, "\n");
        }

        commit_list_insert(head, &interesting_commits);
        
	    commit_tree
            (
            commit_msg.buf,
            head->tree->object.sha1, 
            interesting_commits, 
            result_commit, 
            NULL, 
            NULL
            );
        strbuf_release(&commit_msg);
        /* Print out in same format as rewrite-parents would */
        printf("%s\n", "HEAD");
        printf("\t%s %s\n", sha1_to_hex(result_commit), sha1_to_hex(head->object.sha1));
        update_ref("subtree split", "HEAD", result_commit, NULL, 0, DIE_ON_ERR);
    } 
    else {
        free_commit_list(interesting_commits);
    }

    return 0;
}

/*---------------------------------------------------------------------------*/
/*                  ######  ####### ######  #     #  #####                   */
/*                  #     # #       #     # #     # #     #                  */
/*                  #     # #       #     # #     # #                        */
/*                  #     # #####   ######  #     # #  ####                  */
/*                  #     # #       #     # #     # #     #                  */
/*                  #     # #       #     # #     # #     #                  */
/*                  ######  ####### ######   #####   #####                   */
/*---------------------------------------------------------------------------*/

static int cmd_subtree_debug(int argc, const char **argv, const char *prefix)
{
    int i;
    const char *split_argv[200];
    struct strbuf **args;
    struct strbuf split_me = STRBUF_INIT;

    //const char *split0 = "subtree split -P red -P blue --rewrite-parents";
    //const char *split1 = "subtree split local-change-to-subtree -P not-a-subtree -P nested/directory --rewrite-parents";
    //const char *split1_resplit = "subtree split split-branch -P not-a-subtree -P nested/directory --rewrite-parents";
    //const char *split1_join = "subtree split local-change-to-subtree -P not-a-subtree -P nested/directory --rejoin";
    //const char *split1_squash = "subtree split local-change-to-subtree -P red -P blue -P not-a-subtree -P nested/directory --rejoin --squash";
    //const char *split2 = "subtree split 140dee624e7c29d65543fffaf28cb0dde2c71e48 -P blue -P red --rewrite-parents --always-create";
    //const char *split3 = "subtree split local-change-to-subtree -P red -P blue --onto 2075c241ce953a8db2b37e0aac731dc60c82a5af --onto b5450a2b82e0072abd37c1791a2bc3810b6e61f0 --footer \nFooter --annotate Split: --always-create";
    //const char *split4 = "subtree split local-change-to-subtree -P not-a-subtree";
    //const char *splitAll = "subtree split -P red -P blue --all --rewrite-parents";
    //const char *add1 = "subtree add -P green green";
    //const char *add2 = "subtree add -P green -r ../green green --squash";
    const char *merge1 = "subtree merge -P green green2";
    //const char *pull1 = "subtree pull -P green ../green HEAD";

    const char *command = merge1;

    strbuf_addstr(&split_me, command);
    args = strbuf_split(&split_me, ' ');
    i = 0;
    while(args[i])
    {
        if(args[i]->buf[args[i]->len-1] == ' ') 
            args[i]->buf[args[i]->len-1] = '\0';
        split_argv[i] = args[i]->buf;
        i++;
    }
    split_argv[i] = 0;
    cmd_subtree(i, split_argv, "");

    return 0;
}

/*---------------------------------------------------------------------------*/
/*            #####  #     # ######  ####### ######  ####### #######         */
/*           #     # #     # #     #    #    #     # #       #               */
/*           #       #     # #     #    #    #     # #       #               */
/*            #####  #     # ######     #    ######  #####   #####           */
/*                 # #     # #     #    #    #   #   #       #               */
/*           #     # #     # #     #    #    #    #  #       #               */
/*            #####   #####  ######     #    #     # ####### #######         */
/*---------------------------------------------------------------------------*/

static const char * const builtin_subtree_usage[] = {
    "git subtree add"
    "git subtree list",
    "git subtree merge",
    "git subtree pull",
    "git subtree push",
    "git subtree reset", /* TODO */
    "git subtree split",
    "git subtree squash", /* TODO */
    NULL
};
                                                 
int cmd_subtree(int argc, const char **argv, const char *prefix)
{
    struct option options[] = {
        OPT_END()
    };

    int result;

    argc = parse_options(argc, argv, prefix, options, builtin_subtree_usage, PARSE_OPT_STOP_AT_NON_OPTION);

    if (argc < 1)
        usage_with_options(builtin_subtree_usage, options);
    else if (!strcmp(argv[0], "add"))
        result = cmd_subtree_add(argc, argv, prefix);
    else if (!strcmp(argv[0], "list"))
        result = cmd_subtree_list(argc, argv, prefix);
    else if (!strcmp(argv[0], "merge"))
        result = cmd_subtree_merge(argc, argv, prefix);
    else if (!strcmp(argv[0], "pull"))
        result = cmd_subtree_pull(argc, argv, prefix);
    else if (!strcmp(argv[0], "push"))
        result = cmd_subtree_push(argc, argv, prefix);
    else if (!strcmp(argv[0], "split"))
        result = cmd_subtree_split(argc, argv, prefix);
    else if (!strcmp(argv[0], "debug"))
        result = cmd_subtree_debug(argc, argv, prefix); /* TODO: Remove */
    else {
        error("Unknown subcommand: %s", argv[0]);
        usage_with_options(builtin_subtree_usage, options);
    }

    return 0;
}

/*-----------------------------------------------------------------------------
                          http://patorjk.com/software/taag/ 
                                      BANNER
                            #     #####   #####  ### ### 
                           # #   #     # #     #  #   #  
                          #   #  #       #        #   #  
                         #     #  #####  #        #   #  
                         #######       # #        #   #  
                         #     # #     # #     #  #   #  
                         #     #  #####   #####  ### ###
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
                       ####### ####### ######  #######                       
                          #    #     # #     # #     #                       
                          #    #     # #     # #     #                       
                          #    #     # #     # #     #                       
                          #    #     # #     # #     #                       
                          #    #     # #     # #     #                       
                          #    ####### ######  #######                       
 -----------------------------------------------------------------------------
* Figure out how to make tab auto complete find branch names, remotes, etc.
* Add subtree reset command?
* Add options to reflog to ignore subtrees
-----------------------------------------------------------------------------*/







                                 

