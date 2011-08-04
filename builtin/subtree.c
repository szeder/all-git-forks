/*
 * Builtin "git subtree" and related commands
 *
 * (C) Copyright 2011 Nick Mayer
 *
 * Based on git-subtree.sh by Avery Pennarun and Jesse Greenwald's subtree
 * patch for jgit.
 */

#include "builtin.h"
#include "subtree.h"
#include "parse-options.h"
#include "color.h"
#include "revision.h"
#include "refs.h"

/*---------------------------------------------------------------------------*/
/*              #####  ####### #     # #     # ####### #     #               */
/*             #     # #     # ##   ## ##   ## #     # ##    #               */
/*             #       #     # # # # # # # # # #     # # #   #               */
/*             #       #     # #  #  # #  #  # #     # #  #  #               */
/*             #       #     # #     # #     # #     # #   # #               */
/*             #     # #     # #     # #     # #     # #    ##               */
/*              #####  ####### #     # #     # ####### #     #               */
/*---------------------------------------------------------------------------*/

static int debug_printf_enabled = 1;
#define debug(...) if (debug_printf_enabled) color_fprintf(stderr, GIT_COLOR_CYAN, __VA_ARGS__)
#define info(...) if (debug_printf_enabled) color_fprintf(stderr, GIT_COLOR_BOLD_GREEN, __VA_ARGS__)
#define warn(...) if (debug_printf_enabled) color_fprintf(stderr, GIT_COLOR_BOLD_RED, __VA_ARGS__)

/*-----------------------------------------------------------------------------
Options parsing for string lists
-----------------------------------------------------------------------------*/
static int opt_string_list(const struct option *opt, const char *arg, int unset)
{
    struct string_list *exclude_list = opt->value;
    string_list_append(exclude_list, arg);
    return 0;
}

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
    const char* remote;
    const char* name;
    const char* prefix;
};

static const char * const builtin_subtree_add_usage[] = {
    "git subtree add -P <prefix> [-n | --name] [(-r | --remote)=<remote>] [--squash] <branch>",
    NULL
};

/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int cmd_subtree_add(int argc, const char **argv, const char *prefix)
{
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

/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int cmd_subtree_list(int argc, const char **argv, const char *prefix)
{
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
    "git subtree merge -P <prefix> [-r | --remote=<remote>] [--squash] <branch> [merge options]",
    NULL
};

struct merge_opts {
    int squash;
    const char *remote;
    struct string_list prefix_list;
};

static int cmd_subtree_merge(int argc, const char **argv, const char *prefix)
{
    return 0;
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
    return 0;
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

static const char * const builtin_subtree_split_usage[] = {
    "git subtree split [(-P <prefix>)...] [options] <paths>",
    "git subtree split [options] <branch> <paths>",
    "git subtree split [options] [<branch>] -- <paths>",
    NULL,
};

struct split_opts {
    int change_committer;
    const char *annotation;
    const char *footer;
    struct string_list prefix_list;
};

struct split_detail {
    struct commit *new_commit;
};

struct split_util_container
{
    int count;
    struct split_detail details[];
};

static struct split_detail *get_split_detail(struct commit *commit, int prefix_index, int create)
{
    struct split_util_container *container = commit->util;
    if (create) {
        if (!container) {
            container = xmalloc(sizeof(struct split_util_container) + sizeof(struct split_detail) * 16);
            container->count = 4;
            memset(container->details, 0, sizeof(*container->details) * container->count);
            commit->util = container;
        }
        while (container->count < prefix_index) {
            container = xrealloc(container, container->count * 2);
            memset(&container->details[container->count], 0, sizeof(*container->details) * container->count);
            container->count *= 2;
            commit->util = container;
        }
    }

    if (container)
        return &container->details[prefix_index];
    else
        return NULL;
}

/*-----------------------------------------------------------------------------
Rewrite the given commit with the tree sha & parents
-----------------------------------------------------------------------------*/
static struct commit *write_commit(struct commit *commit,
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
    
    /* Cleanup */
    strbuf_release(&author_str);
    strbuf_release(&committer_str);
    strbuf_release(&body_str);
    /* commit_tree frees remapped_parents for us */

    info("created commit %s\n", sha1_to_hex(output_commit_sha1));

    return lookup_commit(output_commit_sha1);
}

/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
struct subtree_refs
{
    struct strbuf refname;
    const unsigned char *sha1;
    int flags;
    struct subtree_refs *next;
};

static int get_refs(const char *refname, const unsigned char *sha1, int flags, void *data)
{
    struct subtree_refs **refs = data;
    struct subtree_refs *ref;

    ref = xmalloc(sizeof(*ref));
    strbuf_init(&ref->refname, 0);
    strbuf_addstr(&ref->refname, refname);
    ref->sha1 = sha1;
    ref->flags = flags;
    ref->next = *refs;
    *refs = ref;

	return 0;
}

/*-----------------------------------------------------------------------------

-----------------------------------------------------------------------------*/
static int cmd_subtree_split(int argc, const char **argv, const char *prefix)
{
    struct split_opts opts;
    struct option options[] = {
        OPT_BOOLEAN(0, "committer", &opts.change_committer, "Rewritten commits will use current commiter information"),
        OPT_STRING(0, "annotate", &opts.annotation, "annotation", "Add an annotation to the beginning of the commit message of split commits"),
        OPT_STRING(0, "footer", &opts.footer, "annotation", "Add an annotation to the end of the commit message of split commits"),
        OPT_CALLBACK('P', "prefix", &opts.prefix_list, "prefix", "prefix <path>", opt_string_list),
        OPT_END(),
    };
    struct rev_info rev;
    struct setup_revision_opt opt;
    struct commit *commit;

    /* 
     * Parse arguments. Remaining arguments will be the refspec to perform the split on 
     */
    memset(&opts, 0, sizeof(opts));
    argc = parse_options(argc, argv, prefix, options, builtin_subtree_split_usage, PARSE_OPT_KEEP_UNKNOWN);

    { /* <DEBUG> */
        int i;
        debug("Splitting (%d): ", opts.prefix_list.nr);
        for (i = 0; i < opts.prefix_list.nr; i++)
        {
            debug("\t%s ", opts.prefix_list.items[i]);
        }
        debug("\n");
    } /* </DEBUG> */

    /*
     * Setup the walk. Make sure the user didn't pass any flags that will
     * mess things up
     */
    init_revisions(&rev, prefix);
    rev.topo_order = 1;
    rev.reverse = 1;
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
        struct tree **subtrees;
        int prefix_index = 0;

        debug("Processing %s...\n", sha1_to_hex(commit->object.sha1));
        
        subtrees = subtree_trees(commit, &opts.prefix_list);
        for (prefix_index = 0; prefix_index < opts.prefix_list.nr; prefix_index++) {
            struct tree *tree;

            tree = subtrees[prefix_index];
            if (tree) {
                struct commit *new_commit;
                struct commit *identical_parent = NULL;
                struct commit_list *new_parents = NULL;
                struct commit_list *parent;
                struct commit_list **insert;
                struct split_detail *split_detail;
                debug("\t%s -> %s\n", opts.prefix_list.items[prefix_index].string, sha1_to_hex(tree->object.sha1));

                /* Check the parents of this commit to see if this tree sha has already been written out */
                parent = commit->parents;
                insert = &new_parents;
                while (parent) {
                    split_detail = get_split_detail(parent->item, prefix_index, 0);
                    if (split_detail && split_detail->new_commit) {

                        /* 
                         * If the parent's tree is the same as ours, nothing has changed and we
                         * shouldn't create a new commit.
                         */
                        if (split_detail->new_commit->tree == tree) {
                            identical_parent = split_detail->new_commit;
                        }

                        /* Insert at the end to preserve the same order as the parents */
                        insert = &commit_list_insert(split_detail->new_commit, insert)->next;
                    }
                    parent = parent->next;
                }

                /* 
                 * If this is a new SHA, create a new commit for it. If there is a parent with 
                 * the same identical tree, just use it instead (don't create an empty commit)
                */
                if (!identical_parent) {
                    new_commit = write_commit(commit, tree, new_parents, 1, &opts);
                    parse_commit(new_commit);
                }
                else {
                    new_commit = identical_parent;
                }

                /* Store the new commit in the util of the commit so descendants will know */
                get_split_detail(commit, prefix_index, 1)->new_commit = new_commit;
            }
        }
        free(subtrees);
    
    }
    
    /* 
     * If requested, walk through the refs and create them on the new commits as 
     * refs/subtree/<prefix>/<ref> 
     */
    {
        struct subtree_refs *refs = NULL;
        for_each_ref_in("refs/heads/", get_refs, &refs);
        while (refs) {
            struct commit *commit;
            struct split_detail *split_detail;
	        struct ref_lock *lock = NULL;
            int prefix_index = 0;
            struct subtree_refs *next = refs->next;
            
            commit = lookup_commit(refs->sha1);

            for (prefix_index = 0; prefix_index < opts.prefix_list.nr; prefix_index++) {
                split_detail = get_split_detail(commit, prefix_index, 0);

                info("Create branch %s/%s at %s\n", opts.prefix_list.items[prefix_index].string, refs->refname.buf, 
                    (split_detail && split_detail->new_commit) ? sha1_to_hex(split_detail->new_commit->object.sha1) : "NA");
                if (split_detail && split_detail->new_commit) {
                    struct strbuf branch_head = STRBUF_INIT;
                
                    strbuf_addstr(&branch_head, "refs/subtree/");
                    strbuf_addstr(&branch_head, opts.prefix_list.items[prefix_index].string);
                    strbuf_addstr(&branch_head, "/");
                    strbuf_addstr(&branch_head, refs->refname.buf);

                    lock = lock_any_ref_for_update(branch_head.buf, NULL, 0);
                    if (write_ref_sha1(lock, split_detail->new_commit->object.sha1, "subtree") < 0)
                        die_errno("Failed to write ref");
                }
            }

            strbuf_release(&refs->refname);
            free(refs);
            refs = next;
        }
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

    const char *command = "subtree split -P folder1 -P folder2 --all";

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
    "git subtree add",
    "git subtree list",
    "git subtree merge",
    "git subtree pull",
    "git subtree push",
    "git subtree reset",
    "git subtree split",
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
    else if (!strcmp(argv[0], "split"))
        result = cmd_subtree_split(argc, argv, prefix);

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
    else if (!strcmp(argv[0], "reset"))
        result = 0;
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
-------------------------------------------------------------------------------
* 
-----------------------------------------------------------------------------*/
