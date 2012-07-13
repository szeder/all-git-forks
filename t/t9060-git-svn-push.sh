#!/bin/sh

test_description='git svn-push basic'
. ./lib-git-svn-fetch.sh

test_expect_success 'push init commit' '
	git svn-fetch -v --user user --pass pass $svnurl &&
	test
'
