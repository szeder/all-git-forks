#!/bin/sh
#
# Copyright (c) 2011 Ray Chen
#

test_description='git svn test (option --preserve-empty-dirs)

This test uses git to clone a Subversion repository that contains empty
directories, and checks that corresponding directories are created in the
local Git repository with placeholder files.'

. ./lib-git-svn.sh

say 'define NO_SVN_TESTS to skip git svn tests'
GIT_REPO=git-svn-repo

test_expect_success 'initialize source svn repo containing empty dirs' '
	svn_cmd mkdir -m x "$svnrepo"/trunk "$svnrepo"/tags &&
	svn_cmd co "$svnrepo"/trunk "$SVN_TREE" &&
	(
		cd "$SVN_TREE" &&
		mkdir -p module/foo module/bar &&
		echo x >module/foo/file.txt &&
		svn_cmd add module &&
		svn_cmd commit -mx &&

		svn_cmd mv module/foo/file.txt module/bar/file.txt &&
		svn_cmd commit -mx &&
		svn_cmd cp "$svnrepo"/trunk "$svnrepo"/tags/v1.0 -m"create standard tag" &&
		svn_cmd cp "$svnrepo"/trunk/module "$svnrepo"/tags/module_v1.0 -m"create non-standard tag" &&

		svn_cmd rm -m"remove non-standard tag" "$svnrepo"/tags/module_v1.0 &&

		svn_cmd cp "$svnrepo"/trunk/module "$svnrepo"/tags/module_v1.0 -m"re-create non-standard tag" &&

    true
	) &&
	rm -rf "$SVN_TREE"
'

test_expect_success 'clone svn repo with --preserve-empty-dirs --stdlayout' '
	git svn clone "$svnrepo" --preserve-empty-dirs --stdlayout "$GIT_REPO"
'
exit 1
test_done
