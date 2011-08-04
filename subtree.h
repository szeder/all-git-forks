#ifndef SUBTREE_H
#define SUBTREE_H

#include "commit.h"
#include "object.h"
#include "string-list.h"

struct commit_list *subtree_commits_with_prefixes(int argc, 
                                                  const char **argv, 
                                                  const char *prefix,
                                                  struct string_list *prefix_list);

												  
struct tree **subtree_trees(struct commit *commit, 
                                 struct string_list *prefix_list);

#endif /* SUBTREE_H */