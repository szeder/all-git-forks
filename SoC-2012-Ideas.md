This page contains project ideas for Google Summer of Code 2012 from the
Git user and development community.

If you're active in the git community and have an idea that you would
feel comfortable mentoring, feel free to add it. If you have an idea but
need to find a suitable mentor, please bring it up on the [git mailing
list]; others can help you develop the idea and may volunteer to mentor.

If you're a prospective GSoC student, read through the ideas and see if
any interest you. But note that these are ideas, not project proposals;
they may need details filled in or expanded to make a good project.
Find an area that interests you and start a discussion on the [git
mailing list], even if it's just by asking more about the topic. A good
proposal will be clear about the problem to be solved, the history of
work in that area, and the specifics of the approach that the GSoC
project will take. You can find some of those answers by reading the
code and searching the list archives, but discussing the idea with
interested developers is a great way for both the student and mentor to
reach an understanding of exactly what is to be accomplished.

[git mailing list]: https://git.wiki.kernel.org/articles/g/i/t/GitCommunity_c4e3.html#Mailing_List

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

Modernizing and expanding Git.pm
--------------------------------

Git.pm was created in 2006 to make it easy to call git commands from
Perl scripts safely, in a portable way (including workarounds required
for ActiveState Perl on MS Windows).  Its error handling via exceptions
also comes from that year.

Git.pm module uses Error (and Error::Simple) for its exception handling.
Unfortunately, while it might looked like a good choice in 2006, Error
module is deprecated:

> WARNING
>
> Using the "Error" module is **no longer recommended** due to the
> black-magical nature of its syntactic sugar, which often tends to
> break. Its maintainers have stopped actively writing code that uses
> it, and discourage people from doing so. See the "SEE ALSO" section
> below for better recommendations.

Nowadays the recommended solution to exception handling in Perl are
Try::Tiny (or TryCatch, but I don't think Git.pm would need this more
heavyweight module) for capturing and handling exceptions, and
Exception::Class (or Throwable, but that requires heavyweight Moose
object system) for throwing OO exceptions.

The major goal would be to update Git.pm to modern Perl conventions,
amon others moving from Error / Error::Simple to Try::Tiny and
Exception::Class, preserving backwards compatibility, but perhaps also
adding a better interface and using it in git commands implemented in
Perl.

Other optional goals would be to extend Git.pm, for example adding
Git::Config module which would read git configuration once like gitweb
does, or Git::Commit module for parsing commit objects, etc.

Programming language: Perl  
Proposed by: Jakub Narębski  
Possible mentor(s): Jakub Narębski (?)

Use JavaScript library / framework in gitweb
--------------------------------------------

Gitweb (git web interface) includes some *optional* client-side
scripting using JavaScript.  This includes checking if JavaScript is
available and remembering this information so gitweb can choose
JavaScript-only version of a view (javascript-detection), selecting
common timezone to use when showing dates (adjust-timezone), and
AJAX-y incremental blame view (blame_incremental).

Currently all this is done using hand-written JavaScript.  This means
that gitweb scripting includes handling cookies, formatting output,
processing dates, and smoothing out incompatibilities between browsers
(like e.g. XmlHttpRequest creation).

This is redoing work which JavaScript libraries, such as jQuery,
MooTools or YUI already did.  Moreover, if we want to add new features
(e.g. table sorted using JavaScript), or improve existing ones, we
would have to re-implement existing JavaScript code.  Also our
hand-crafted code is not as well tested as widely used JavaScript
libraries.

The goal of this project is to move gitweb client side scripting to
use some JavaScript library / JavaScript framework.

The project would consist of the following steps:

 * Add support for configuring and loading external JavaScript library
   to `gitweb/gitweb.perl` and `gitweb/Makefile`.  It would be nice
   (though not necessary) to be able to use local version of library,
   and have such feature well documented.

 * Remove gitweb's JavaScript mini-library in `gitweb/static/js/lib`
   and replace it part by part by appropriate JavaScript library
   functions (methods).

 * Replace DOM selectors by library version, if applicable.

 * Optional: emulate 'onprogress' in XmlHttpRequest using native
   JavaScript library mechanism (creating a class, or whatever).

 * Optional: better deferring of repainting in incremental blame.

Note that we require that client-side scripting in gitweb follow
[progressive enhancement] strategy; gitweb should work correctly,
perhaps with reduced functionality, even if JavaScript is turned off,
or external JavaScript library cannot be loaded.

[progressive enhancement]: http://en.wikipedia.org/wiki/Progressive_enhancement

Programming language: JavaScript  
Proposed by: Jakub Narębski  
Possible mentor(s): Jakub Narębski

`git instaweb --serve`
----------------------

[git-instaweb] is a tool for browsing a repository (in gitweb) with
a web browser.  To use it, simply run `git instaweb` inside
repository.  It would set up gitweb and a web server, and by default
also run a web browser.  Web interface would be available at
`http://localhost:1234`.

For informal, ad-hoc sharing it would be nice if there was an option
to `git instaweb` that would make it also allow remote machines
to **pull** from you (via HTTP), similar to [hg serve] command in
Mercurial version control system.

git-instaweb supports many web servers.  Currently apache2, lighttpd,
mongoose, plackup and webrick are supported.  It is not necessary to
implement support for `--serve` in all of them.

The project would consist of the following steps:

 * Configure web server to run `git-http-backend` CGI program to serve
   git repositories over HTTP.  It should probably allow by default
   only read-only use.

 * Configure gitweb to show fetch URL in 'summary' page.

 * If possible for given web server (and with available modules),
   configure it so that "smart" HTTP server and gitweb share common
   URL (are available under the same URL).

 * If possible use mechanism native for a web server used, for example 
   [Plack::App::GitSmartHttp] for 'plackup' web server, or [grack]
   with WEBrick.

 * Optionally: add support for pushing (disabled by default).

 * Optionally: add documentation about using `git instaweb` to
   "[Git User's Manual]" (or one of tutorials), similar to appropriate
   chapter in [hgbook] ("Mercurial: The Definitive Guide").

The minimum would be to implement pull support under different URL
than web interface, and only for one web server.

Programming language: shell script  
Proposed by: Jakub Narębski  
Possible mentor(s): Jakub Narębski, Eric Wong, ...

[git-instaweb]: http://schacon.github.com/git/git-instaweb.html
[hg serve]: http://mercurial.selenic.com/wiki/hgserve
[Plack::App::GitSmartHttp]: http://search.cpan.org/perldoc?Plack::App::GitSmartHttp
[grack]: https://github.com/schacon/grack
[Git User's Manual]: http://schacon.github.com/git/user-manual.html
[hgbook]: http://hgbook.red-bean.com/read/collaborating-with-other-people.html#sec:collab:serve

Finishing network support for libgit2
-------------------------------------

The library currently has support for fetching code over the http and git protocols. The aim of this project is to finish the support for the remaining network operations:

Major goals:

- Fetch over SSH, including a sane API for managing the user's SSH keys.
- Push over http, git, and SSH, with pack-objects as a prerequisite.

This is not as terribly complicated as it looks, because the existing networking code is well modularized and easy to extend: most of the required sockets functionality is already in place.

Programming language: C89  
Possible mentor(s): Vicent Marti, Russell Belfer

Teaching "--3way" to "git apply"
--------------------------------

The "-3" option "git am" understands is useful only when you are
applying a full format-patch submission. Teaching the three-way
fallback to underlying "git apply" would make the feature avialable in
more use cases, and later can help making the implementation of "git
am -3" simpler.

Programming language: C89  
Suggested by: Junio C Hamano  
Possible mentors: ???  

Complete "Linus's ultimate content tracking tool"
-------------------------------------------------

Early in the Git development history, Linus envisioned an "ultimate content tracking tool"
in a [message](http://thread.gmane.org/gmane.comp.version-control.git/217). Starting from
the current codebase, it would dig deeper in the history to answer "where did this line
come from?" and even find out:

> "oops, that line didn't even exist in the previous version, BUT I
> FOUND FIVE PLACES that matched almost perfectly in the same diff,
> and here they are"

We already have the part to dig through the history to find the commit that introduced the
line in question as "git rev-list -S<the contents on the line>", but we do not have any tool
to help the "BUT I FOUND FIVE PLACES that matched almost perfectly" part.

Write a tool that can be used for the task, and optionally wrap an interactive UI around it.

Programming language: Any  
Possible mentors: ???  

Graphical diff in git-gui
-------------------------

[git-gui] is a portable Tcl/Tk based graphical interface to Git,
focused on commit generation and single file annotation.  It is part
of Git, even though it is developed in a [separate repository].

git-gui can show differences as syntax-highlighted unified diff.  The
goal of this project would be to add graphical side-by-side diff.
One possibility is to make use of [TkDiff] code (a graphical diff and
merge tool), which is also GPL licensed.

The project would consist of the following steps (not all must be
implemented during Google Summer of Code):

 * Add "git gui diff" subcommand, which would show single file
   side-by-side graphical compare or/and graphical diff.

   Graphical part and code for side-by-side compare (showing full
   contents of both files) can be taken from TkDiff, while graphical
   side-by-side diff (showing changes plus context) can be translated
   from gitweb's side-by-side diff code.

 * Integrate graphical diff with main git-gui application (including
   switching between side-by-side and unified diff).

 * Add highlighting of changes in diff both to side-by-side
   (new code), and to unified diff (current code).

   It can be done using code for TkDiff (perhaps with Git performing
   word diff instead of doing it in Tcl), or/and using algorithm from
   gitweb and diff-highlight in contrib.  The difference is that one
   uses word diff or character diff to highlight changes, the other
   just skips common prefix and suffix.

 * Add graphical merge / graphical 3-way diff support.  Both are to be
   used in case a file has textual conflicts; graphical merge also
   includes resolving a merge by taking 'our' side, 'their' side (or
   optionally also 'ancestor' side), or by editing merge result.

 * Graphical side-by-side tree level diff, or side-by-side directory
   listing with differences highlighted.

   UI can be taken from two-panel filemanagers (like MC, or Total
   Commander), or from synchronization tools (like e.g. Unison).

   The difficulty can be in showing rename and copy detection results,
   and in showing type (filemode) changes.

The minimum would be to implement side-by-side diff or side-by-side
compare of two files, without highlighting changes (diff refinement
highlighting) in the form of separate "`git gui diff <file>`"
command.

[git-gui]: http://schacon.github.com/git/git-gui.html
[separate repository]: http://repo.or.cz/w/git-gui.git
[TkDiff]: http://freecode.com/projects/tkdiff

Programming language: Tcl/Tk  
Proposed by: Jakub Narębski  
Possible mentor(s): Pat Thoyts, Paul Mackerras (?)

Other sources of inspiration
----------------------------

* Previous year's SoC ideas:
[SoC2011Ideas](https://git.wiki.kernel.org/articles/s/o/c/SoC2011Ideas_49fd.html),
[SoC2010Ideas](https://git.wiki.kernel.org/articles/s/o/c/SoC2010Ideas_ccd4.html)
* [Git users survey](http://permalink.gmane.org/gmane.comp.version-control.git/183242)
* [Small project ideas](https://git.wiki.kernel.org/articles/s/m/a/SmallProjectsIdeas_00e5.html)
(probably too small for a SoC)
