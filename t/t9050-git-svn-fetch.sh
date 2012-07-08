#!/bin/sh

test_description='git svn-fetch non trunk'
. ./lib-git-svn-fetch.sh

test_expect_success 'empty repo' '
	svn_cmd mkdir -m "empty dir" $svnurl/empty-dir
'

test_expect_success 'fetch empty repo' '
	git svn-fetch --user user --pass pass $svnurl
'

test_expect_success 'checkout empty repo' '
	test_must_fail git checkout svn/trunk
'

test_expect_success 'init repo' '
	svn_cmd co $svnurl svnco &&
	cd svnco &&
	echo "some contents" > file.txt &&
	svn_cmd add file.txt &&
	svn_cmd ci -m "some commit" &&
	cd ..
'

date=`svn_date HEAD svnco`

test_expect_success 'fetch non-empty repo' '
	git svn-fetch --user user --pass pass $svnurl &&
	git checkout svn/trunk &&
	test_file file.txt "some contents" &&
	test_subject HEAD "some commit" &&
	test_author HEAD "Full Name <mail@example.com>" &&
	test_date HEAD $date
'

test_done

