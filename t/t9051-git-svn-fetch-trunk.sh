#!/bin/sh

test_description='git svn-fetch trunk'
. ./lib-git-svn-fetch.sh

test_expect_success 'setup branches' 'setup_branches'

test_expect_success 'trunk' '
	git svn-fetch -v --user user -t Trunk $svnurl &&
	git checkout svn/trunk &&
	test_file file.txt "other" &&
	test_subject HEAD "trunk file" &&
	test_subject HEAD~1 "init"
'

test_done
