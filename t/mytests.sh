#!/bin/sh
#
# Copyright (c) 2007 Lars Hjemli
#

test_description='Test updating submodules
'

. ./test-lib.sh

test_expect_success 'push with submodules without remote' '
	mkdir a &&
	(cd a &&
	 git init --bare
	) &&
	git clone a a1 &&
	(cd a1 &&
	 mkdir b &&
	 (cd b &&
	  git init &&
	  >junk &&
	  git add junk &&
	  git commit -m "initial"
	 ) &&
	 git add b &&
	 git commit -m "added submodule" &&
	 git push origin master
	)
'

test_done
