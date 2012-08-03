#!/bin/sh

test_description='git svn-fetch non trunk'
. ./lib-git-svn-fetch.sh

test_expect_success 'fetch empty' '
	git svn-fetch -v &&
	test_must_fail test -x .git/refs/svn/latest &&
	test_must_fail git checkout svn/master &&
	test_must_fail git checkout svn/trunk
'

test_expect_success 'init repo' '
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
	test $(GIT_SVN_FETCH_REPORT_LATEST=1 git svn-fetch) -eq 1 &&
	git checkout svn/master &&
	test -d empty-dir &&
	test -e empty-dir/.gitempty &&
	test_file file.txt "some contents" &&
	test_git_subject HEAD "some commit" &&
	test_git_author HEAD "C O Mitter <committer@example.com>" &&
	test_git_date HEAD $date
'

test_expect_success 'auto crlf' '
	cd svnco &&
	echo -e "foo\r\nbar\r" > crlf.txt &&
	svn_cmd add crlf.txt &&
	svn_cmd ci -m "crlf" &&
	cd .. &&
	echo "* text=auto" > .git/info/attributes &&
	git config svn.eol crlf &&
	git config core.eol lf &&
	git svn-fetch -v &&
	git checkout svn/master &&
	test_file crlf.txt "$(echo -e "foo\nbar")"
'

test_done

