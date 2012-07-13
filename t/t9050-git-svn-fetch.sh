#!/bin/sh

test_description='git svn-fetch non trunk'
. ./lib-git-svn-fetch.sh

test_expect_success 'fetch empty' '
	git svn-fetch -v &&
	test_must_fail test -x .git/svn-latest &&
	test_must_fail git checkout svn/trunk
'

test_expect_success 'init repo' '
	svn_cmd co $svnurl svnco &&
	cd svnco &&
	svn_cmd mkdir empty-dir &&
	echo "some contents" > file.txt &&
	svn_cmd add file.txt &&
	svn_cmd ci -m "some commit" &&
	cd ..
'

date=`svn_date HEAD svnco`

test_expect_success 'fetch repo' '
	git svn-fetch -v &&
	test_file .git/svn-latest 1 &&
	git checkout svn/trunk &&
	test -d empty-dir &&
	test -e empty-dir/.gitempty &&
	test_file file.txt "some contents" &&
	test_subject HEAD "some commit" &&
	test_author HEAD "Full Name <mail@example.com>" &&
	test_date HEAD $date
'

test_expect_success 'auto crlf' '
	cd svnco &&
	echo -e "foo\r\nbar\r" > crlf.txt &&
	svn_cmd add crlf.txt &&
	svn_cmd ci -m "crlf" &&
	cd .. &&
	echo "* text=auto" > .git/info/attributes &&
	git config core.eol crlf &&
	git svn-fetch -v &&
	git config core.eol lf &&
	git checkout svn/trunk &&
	test_file crlf.txt "$(echo -e "foo\nbar")"
'

test_done

