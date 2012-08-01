#!/bin/sh

test_description='git svn-push'
. ./lib-git-svn-fetch.sh

test_expect_success 'init push' '
	echo "foo" > file.txt &&
	git add file.txt &&
	git commit -a -m "initial commit" &&
	git svn-push -v master 00000 master &&
	cd svnco &&
	svn up &&
	test_svn_subject HEAD "initial commit" &&
	test_svn_author HEAD committer &&
	test_file file.txt "foo" &&
	cd ..
'

test_expect_success 'multiple commits' '
	echo "bar" >> file.txt &&
	git commit -a -m "second commit" &&
	mkdir a &&
	echo "fefifofum" >> a/test &&
	git add a/test &&
	git commit -a -m "third commit" &&
	git svn-push -v master svn/master master &&
	cd svnco &&
	svn up -r 2 &&
	test_svn_subject BASE "second commit" &&
	test_svn_author BASE committer &&
	test_file file.txt $(echo -e "foo\nbar") &&
	test ! -e a &&
	svn up -r 3 &&
	test_svn_subject BASE "third commit" &&
	test_svn_author BASE committer &&
	test_file a/test "fefifofum" &&
	cd ..
'

test_done

