This page contains project ideas for Google Summer of Code 2012 from the
Git user and development community. You can get started by reading some
project descriptions, and the mailing list thread(s) that spawned them.
If you have another idea, add it to this page and start a discussion on
the [[git mailing
list|https://git.wiki.kernel.org/articles/g/i/t/GitCommunity_c4e3.html#Mailing_List]].

Better git log --follow support
-------------------------------

When showing the history of a subset of a project (e.g., `git log --
foo.c`), git shows only the history of changes that affects the single pathname.
Because of this, changes made to the content currently at
`foo.c` that was previously called `bar.c` will not be shown.

We have the `--follow` option that switches the path to follow to `bar.c`
by following renames, but it has some deficiencies.

For example, it follows only a single path, and the path it follows is
global, which means that you cannot follow multiple lines of development
in which the file was renamed on one side and and not the other.  Also,
it does not interact well with git's usual history simplification (which
displays a connected subgraph of the history that pertains to `foo.c`).

Major topics:

 * Expand --follow to handle arbitrary pathspecs

 * Design and implement a new architecture for --follow that will allow
   it to mark uninteresting commits as part of the usual history
   simplification process. Note that care must be taken not to impact the
   performance of non-follow codepaths.

Proposed mentor: Jeff King

Better big-file support
-----------------------

Git generally assumes that the content being stored in it is source
code, or some other form of text approximately the same size. While git
can handle arbitrary-sized binary content, its base assumptions
sometimes mean some operations are slow or unnecessary space is consumed
for large binary files (e.g., videos or other media).

There has been some work in the last year on streaming files into and
out of the object database when possible. However, there is still more
work to be done:

 * Some code paths still load large files into memory. Some other code
   paths may convert in-pack large files into loose format. These code
   paths need to be analyzed and fixed.

 * Some code paths, such as diff, require files in memory. When files
   are not text (as most large files are not), we can often skip this
   step. However, we sometimes load the whole file just to determine
   that it is binary, defeating the purpose.

 * Many large files (like media) do not delta very well. However, some
   do (like VM disk images). Git could split large objects into smaller
   chunks, similar to bup, and find deltas between these much more
   manageable chunks. There are some preliminary patches in this
   direction, but they are in need of review and expansion.

 * Git copies and stores every object from a remote repository when
   cloning. For large objects, this can consume a lot of bandwidth and
   disk space, especially for older versions of large objects which are
   unlikely to be accessed. Git could learn a new alternate repository
   format where these seldom-used objects are stored on a remote server
   and only accessed on demand.

Proposed mentor: Jeff King

Improving parallelism in various commands
-----------------------------------------

Git is mostly written single-threaded, with a few commands having
bolted-on extensions to support parallel operation (notably git-grep,
git-pack-objects and the core.preloadIndex feature).

We have recently looked into some of these areas and made a few
optimizations, but a big roadblock is that pack access is entirely
single-threaded.  The project would consist of the following steps:

 * In preparation (the half-step): identify commands that could
   benefit from parallelism.  `git grep --cached` and `git grep
   COMMIT` come to mind, but most likely also `git diff` and `git log
   -p`.  You can probably find more.

 * Rework the pack access mechanisms to allow the maximum possible
   parallel access.

 * Rework the commands found in the first step to use parallel pack
   access if possible.  Along the way, document the improvements with
   performance tests.

The actual programming must be done in C using pthreads for obvious
reasons.  At the very least you should not be scared of low-level
programming.  Prior experience and access to one or more multi-core
computers is a plus.

Proposed by: Thomas Rast  
Possible mentor(s): Thomas Rast

Designing a faster index format
-------------------------------

Git is pretty slow when managing huge repositories in terms of files
in any given tree, as it needs to rewrite the index (in full) on
pretty much every operation.  For example, even though _logically_
`git add already_tracked_file` only changes a single blob SHA-1 in the
index, Git will verify index correctness during loading and recompute
the new hash during writing _over the whole index_.  It thus ends up
spending a large amount of time simply on hashing the index.

A carefully designed index format could help in several ways.  (For the
complexity estimates below, let n be the number of index entries or
the size of the index, which is roughly the same.)

 * The work needed for something as simple as entering a new blob into
   the index, which is possibly the most common operation in git
   (think `git add -p` etc.) should be at most log(n).

 * The work needed for a more complex operation that changes the
   number of index entries will have to be larger unless we get into
   database land.  However the amount of data that we SHA-1 over
   should still be log(n).

 * It may be possible to store the cache-tree data directly as part of
   the index, always keeping it valid, and using that to validate
   index consistency throughout.  If so, this would be a big boost to
   other git operations that currently suffer from frequent cache-tree
   invalidation.

Note that there are other criteria than speed: the format should also
be as easy to parse as possible, so as to simplify work for the other
.git-reading programs (such as jgit and libgit2).  For the same
reason, you will also have to show a significant speed boost as
otherwise the break in compatibility is not worth the fallout.

The programming work will be in C, as it replaces a core part of git.

Proposed by: Thomas Rast  
Possible mentor(s): Thomas Rast

Improving the `git add -p` interface
------------------------------------

The interface behind `git {add|commit|stash|reset} {-p|-i}` is shared
and called `git-add--interactive.perl`.    This project would mostly
focus on the `--patch` side, as that seems to be much more widely
used; however, improvements to `--interactive` would probably also be
welcome.

The `--patch` interface suffers from some design flaws caused largely
by how the script grew:

 * Application is not atomic: hitting Ctrl-C midway through patching
   may still touch files.

 * The terminal/line-based interface becomes a problem if diff hunks
   are too long to fit in your terminal.

 * Cannot go back and forth between files.

 * Cannot reverse the direction of the patch.

 * Cannot look at the diff in word-diff mode (and apply it normally).

Due to the current design it is also pretty hard to add these features
without adding to the mess.  Thus the project consists of:

 * Come up with more ideas for features/improvements and discuss them
   with users.

 * Cleanly redesigning the main interface loop to allow for the above
   features.

 * Implement the new features.

As the existing code is written in Perl, that is what you will use for
this project.

Proposed by: Thomas Rast  
Possible mentor(s): Thomas Rast

Remote helper for Subversion
------------------------------------

Write a remote helper for Subversion. While a lot of the underlying
infrastructure work was completed last year, the remote helper itself
is essentially incomplete. Major work includes:

* Understanding revision mapping and building a revision-commit mapper.

* Working through transport and fast-import related plumbing, changing
  whatever is necessary.

* Getting an Git-to-SVN converter merged.

* Building the remote helper itself.

Goal: Build a full-featured bi-directional `git-remote-svn` and get it
      merged into upstream Git.  
Language: C  
See: [A note on SVN history][SVN history], [svnrdump][].  
Proposed by: David Barr  
Possible mentors: Jonathan Nieder, Sverre Rabbelier, David Barr

[SVN history]: http://article.gmane.org/gmane.comp.version-control.git/150007  
[svnrdump]: http://svn.apache.org/repos/asf/subversion/trunk/subversion/svnrdump
