#!/bin/sh
#
# Copyright (c) 2014 Alfred Perlstein
#

test_description='git svn propset tests'

. ./lib-git-svn.sh

foo_subdir2="subdir/subdir2/foo_subdir2"

mkdir import
(cd import
	mkdir subdir
	mkdir subdir/subdir2
	touch foo		# for 'add props top level'
	touch subdir/foo_subdir # for 'add props relative'
	touch "$foo_subdir2"	# for 'add props subdir'
	svn_cmd import -m 'import for git svn' . "$svnrepo" >/dev/null
)
rm -rf import

test_expect_success 'initialize git svn' 'git svn init "$svnrepo"'
test_expect_success 'fetch revisions from svn' 'git svn fetch'

# There is a bogus feature about svn propset which means that it will only
# be taken as a delta for svn dcommit iff the file is also modified.
# That is fine for now.
test_expect_success 'add props top level' '
	git svn propset svn:keywords "FreeBSD=%H" foo &&
	echo hello >> foo &&
	git commit -m "testing propset" foo &&
	git svn dcommit
	svn_cmd co "$svnrepo" svn_project &&
	(cd svn_project && test "`svn propget svn:keywords foo`" = "FreeBSD=%H") &&
	rm -rf svn_project
	'

test_expect_success 'add multiple props' '
	git svn propset svn:keywords "FreeBSD=%H" foo &&
	git svn propset fbsd:nokeywords yes foo &&
	echo hello >> foo &&
	git commit -m "testing propset" foo &&
	git svn dcommit
	svn_cmd co "$svnrepo" svn_project &&
	(cd svn_project && test "`svn propget svn:keywords foo`" = "FreeBSD=%H") &&
	(cd svn_project && test "`svn propget fbsd:nokeywords foo`" = "yes") &&
	(cd svn_project && test "`svn proplist -q foo | wc -l`" -eq 2) &&
	rm -rf svn_project
	'

test_expect_success 'add props subdir' '
	git svn propset svn:keywords "FreeBSD=%H" "$foo_subdir2" &&
	echo hello >> "$foo_subdir2" &&
	git commit -m "testing propset" "$foo_subdir2" &&
	git svn dcommit
	svn_cmd co "$svnrepo" svn_project &&
	(cd svn_project && test "`svn propget svn:keywords "$foo_subdir2"`" = "FreeBSD=%H") &&
	rm -rf svn_project
	'

test_expect_success 'add props relative' '
	(cd subdir/subdir2 && git svn propset svn:keywords "FreeBSD=%H" ../foo_subdir ) &&
	echo hello >> subdir/foo_subdir &&
	git commit -m "testing propset" subdir/foo_subdir &&
	git svn dcommit
	svn_cmd co "$svnrepo" svn_project &&
	(cd svn_project && test "`svn propget svn:keywords subdir/foo_subdir`" = "FreeBSD=%H") &&
	rm -rf svn_project
	'
test_done
