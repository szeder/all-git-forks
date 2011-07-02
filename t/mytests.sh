#!/bin/sh
#
# Copyright (c) 2009 Red Hat, Inc.
#

test_description='Test updating submodules

This test verifies that "git submodule update" detaches the HEAD of the
submodule and "git submodule update --rebase/--merge" does not detach the HEAD.
'

. ./test-lib.sh


compare_head()
{
    sha_master=`git rev-list --max-count=1 master`
    sha_head=`git rev-list --max-count=1 HEAD`

    test "$sha_master" = "$sha_head"
}
#test_expect_success 'submodule vs submodule' '
#	mkdir a &&
#	(cd a &&
#	 git init --bare
#	) &&
#	mkdir b &&
#	(cd b &&
#	 git init --bare
#	) &&
#	git clone b b1 &&
#	(cd b1 &&
#	 echo "submodule" >junk &&
#	 git add junk &&
#	 git commit -m "b: initial commit" &&
#	 git push origin master
#	) &&
#	git clone a a1 &&
#	(cd a1 &&
#	 echo "first" >junk &&
#	 git add junk &&
#	 git submodule add ../b b &&
#	 git submodule update &&
#	 git commit -m "initial commit" &&
#	 (cd b &&
#	  echo "second" >junk &&
#	  git add junk &&
#	  git commit -m "second commit"
#	 )
# 	 git add b
#	 git commit -m "s commit" &&
#	 git checkout HEAD~
#	 bash
#	)
#'

#test_expect_success 'submodule vs nothing' '
#	mkdir a &&
#	(cd a &&
#	 git init --bare
#	) &&
#	mkdir b &&
#	(cd b &&
#	 git init --bare
#	) &&
#	git clone b b1 &&
#	(cd b1 &&
#	 echo "submodule" >junk &&
#	 echo "submodule" >junk1 &&
#	 git add junk &&
#	 git add junk1 &&
#	 git commit -m "b: initial commit" &&
#	 git push origin master
#	) &&
#	git clone a a1 &&
#	(cd a1 &&
#	 echo "first" >junk &&
#	 git add junk &&
#	 git commit -m "initial commit" &&
#	 git submodule add ../b b &&
#	 git submodule update &&
#	 git commit -m "added submodule" &&
#	 git checkout HEAD~
#	)
#'

test_expect_failure 'dir vs submodule' '
	mkdir a &&
	(cd a &&
	 git init --bare
	) &&
	mkdir b &&
	(cd b &&
	 git init --bare
	) &&
	git clone a a1 &&
	(cd a1 &&
	 echo "first" >junk &&
	 git add junk &&
	 mkdir b &&
	 (cd b &&
	  echo "super" >junk
	 ) &&
	 git commit -m "a: initial commit" &&
	 git push origin master
	) &&
	git clone b b1 &&
	(cd b1 &&
	 echo "submodule" >junk &&
	 git add junk &&
	 git commit -m "b: initial commit" &&
	 git push origin master
	) &&
	(cd a1 &&
	 rm -rf b &&
	 git submodule add ../b b &&
	 mkdir c &&
	 (cd c &&
	  >junk
	 ) &&
	 git add c/junk &&
	 echo "second" >junk &&
	 git commit -m "added submodule" &&
	 echo "#######################################################" &&
	 git checkout HEAD~ &&
	 ls &&
	 bash
	)
'
test_done
