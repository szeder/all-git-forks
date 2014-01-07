#!/bin/sh
#
# Copyright (c) 2014 Francesco Pretto
#

test_description='Support for submodules with attached head

This test verifies the sanity of the add and update git submodule commands with
or without the --attached-update, --attach, --detach switches or the
submoudule.<module>.attach property set
'

TEST_NO_CREATE_REPO=true
. ./test-lib.sh

submodurl1=$(pwd -P)/repo1
submodurl2=$(pwd -P)/repo2
repourl=$(pwd -P)/repo

test_expect_success 'setup - create repository "repo1" to be used as submodule' '
	mkdir repo1 &&
	(
		cd repo1 &&
		git init &&
		git config receive.denyCurrentBranch ignore &&
		echo a >a &&
		git add a &&
		git commit -m "repo1 commit 1"
	)
'

test_expect_success 'setup - reate repository "repo2" to be used as submodule' '
	mkdir repo2 &&
	(
		cd repo2 &&
		git init &&
		git config receive.denyCurrentBranch ignore &&
		echo a >a &&
		git add a &&
		git commit -m "repo2 commit 1"
	)
'

test_expect_success 'setup - create repository "repo" to be added with sumodules' '
	mkdir repo &&
	(
		cd repo &&
		git init &&
		git config receive.denyCurrentBranch ignore &&
		echo a >a &&
		git add a &&
		git commit -m "repo commit 1"
	)
'

test_expect_success 'setup - clone repository "repo" in "clonerepo"' '
	git clone "$repourl" clonerepo
'

test_expect_success 'setup - add "mod1" as regular submodule of "repo"' '
	(
		cd repo &&
		git submodule add "$submodurl1" submod1
	)
'

test_expect_success 'setup - add "mod2" as update attached HEAD submodule of "repo"' '
	(
		cd repo &&
		git submodule add --attached "$submodurl2" submod2
	)
'

test_expect_success 'setup - commit submodules in repo' '
	(
		cd repo &&
		git add . &&
		git commit -m "Added submodules"
	)
'

test_expect_success 'init submodules in cloned repo' '
	(
		cd clonerepo &&
		git pull &&
		git submodule init
	)
'

test_expect_success 'update submodules in cloned repo' '
	(
		cd clonerepo &&
		git submodule update
	)
'

test_expect_success 'assert submod1 HEAD is detached in cloned repo' '
	(
		cd clonerepo/submod1 &&
		test "$(git rev-parse --abbrev-ref HEAD)" = "HEAD"
	)
'

test_expect_success 'assert submod2 HEAD is attached in cloned repo' '
	(
		cd clonerepo/submod2 &&
		test "$(git rev-parse --abbrev-ref HEAD)" != "HEAD"
	)
'

test_expect_success 'update submodules with --attach in cloned repo' '
	(
		cd clonerepo &&
		git submodule update --attach
	)
'

test_expect_success 'assert submod1 HEAD is attached in cloned repo' '
	(
		cd clonerepo/submod1 &&
		test "$(git rev-parse --abbrev-ref HEAD)" != "HEAD"
	)
'

test_expect_success 'update submodules with --detach in cloned repo' '
	(
		cd clonerepo &&
		git submodule update --detach
	)
'

test_expect_success 'assert submod1 HEAD is detached in cloned repo' '
	(
		cd clonerepo/submod1 &&
		test "$(git rev-parse --abbrev-ref HEAD)" = "HEAD"
	)
'

test_expect_success 'assert submod2 HEAD is detached in cloned repo' '
	(
		cd clonerepo/submod2 &&
		test "$(git rev-parse --abbrev-ref HEAD)" = "HEAD"
	)
'

test_expect_success 'update submodules in cloned repo (it should do nothing)' '
	(
		cd clonerepo &&
		git submodule update
	)
'

test_expect_success 'assert submod1 HEAD is detached in cloned repo' '
	(
		cd clonerepo/submod1 &&
		test "$(git rev-parse --abbrev-ref HEAD)" = "HEAD"
	)
'

test_expect_success 'assert submod2 HEAD is detached in cloned repo' '
	(
		cd clonerepo/submod2 &&
		test "$(git rev-parse --abbrev-ref HEAD)" = "HEAD"
	)
'

test_expect_success 'setup - add update operation to submodules' '
	(
		cd repo &&
		git config  -f .gitmodules submodule.submod1.update merge &&
		git config  -f .gitmodules submodule.submod2.update rebase &&
		git add . &&
		git commit -m "updated submodules"
	)
'

test_expect_success 'setup - update cloned repo and reinitialize submodules' '
	(
		cd clonerepo &&
		git pull &&
		git submodule init
	)
'

test_expect_success 'add some content to repo2' '
	(
		cd repo2 &&
		echo b >b &&
		git add b &&
		git commit -m "repo2 commit 2"
	)
'

test_expect_success 'update sumodules in cloned repo and verify that submod2 matches repo2' '
	(
		cd clonerepo &&
		git submodule update &&
		test -e submod2/b
	)
'

test_expect_success 'prepend some content to repo1/a' '
	(
		cd repo1 &&
		echo -e "b\na" >a &&
		git add a &&
		git commit -m "repo1 commit 2"
	)
'

test_expect_success 'append some content in clonerepo/submod1 and commit' '
	(
		cd clonerepo/submod1 &&
		echo c >>a &&
		git add a &&
		git commit -m "submod1 commit 1"
	)
'

test_expect_success 'update clonerepo submodules with --attach' '
	(
		cd clonerepo &&
		git submodule update --attach
	)
'

test_expect_success 'verify clonerepo submod1 merge with reattached orphaned commits was correct' '
	(
		cd clonerepo/submod1 &&
		test "$(<a)" = "$'b\na\nc'"
	)
'

test_expect_success 'setup - set operation checkout to submodule sumod1 in repo' '
	(
		cd repo &&
		git config  -f .gitmodules submodule.submod1.update checkout &&
		git add . &&
		git commit -m "updated submodules"
	)
'

test_expect_success 'setup - update cloned repo and reinitialize submodules' '
	(
		cd clonerepo &&
		git pull &&
		git submodule init
	)
'

test_expect_success 'add some content to repo1' '
	(
		cd repo1 &&
		echo b >b &&
		git add b &&
		git commit -m "repo1 commit 3"
	)
'

test_expect_success 'update submodule submod2 (merge ff-only) and verify it matches repo2' '
	(
		cd clonerepo &&
		git submodule update &&
		test -e submod2/b
	)
'

test_done
