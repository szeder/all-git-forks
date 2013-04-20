# Data Structures

A `scoreboard` has the final commit (the starting point, since we moving back in
time).  It has a linked list of `blame_entries`, which themselves contain lists of
contiguous lines.

`blame_entry` is a doubly linked list contiguous sequence of lines that share a
suspect (the blob currently blamed).  The first suspect (of type origin) is
the final commit, usually HEAD.

The origin is a blob (SHA) plus some metadata.  It has a field that contains the
SHA of its owning commit.  SHAs are stored in an encoded format and need to be
decoded by `sha1_to_hex`.

Blame uses `struct object`, a core data structure, whose semantics are, as yet,
unclear, except for the field `sha1`, the object's encoded SHA.


# The Metaphor

An "accused" or "suspect" blob tries to demonstrate (ie "scapegoat") that a
parent of its commit is at "fault" for lines in the range.  This scapegoating
may result in blaming one of its commit's parents for a subset of its lines, in
which case the blame entry is split.

Note:  The metaphor is somewhat muddy because of the use of scapegoat as well as
suspect.


# Issues:

1.  The lines blamed in the blame command may not be contiguous in history.

2.  Line moves 

We need to handle the insertions and deletions anchors of ranges of blamed
lines.

Interesting discovery:  git blame starts at a pseudo-commit which represents 
the working tree, which allows blame to blame changes in a dirty working tree.


# Code

Entry:  `assign_blame`

Main loop:
  
  What is learned, and what actions are taken, between loop iterations?


When are `blame_entries` split?

What's the stopping condition?
