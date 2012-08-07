#!/bin/sh

test_description='git svn-push'
. ./lib-git-svn-fetch.sh

test_expect_success 'init push' '
	echo "foo" > file.txt &&
	git add file.txt &&
	git commit -a -m "initial commit" &&
	git svn-push -v refs/remotes/svn/master $null_sha1 master &&
	cd svnco &&
	svn_cmd up &&
	test_svn_subject "initial commit" &&
	test_svn_author committer &&
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
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	svn_cmd up -r 2 &&
	test_svn_subject "second commit" &&
	test_svn_author committer &&
	echo foo > file_test.txt &&
	echo bar >> file_test.txt &&
	test_file file.txt "$(cat file_test.txt)" &&
	test ! -e a &&
	svn_cmd up -r 3 &&
	test_svn_subject "third commit" &&
	test_svn_author committer &&
	test_file a/test "fefifofum" &&
	cd ..
'

test_expect_success 'remove git empty directories' '
	mkdir -p b/c/d &&
	touch b/c/d/foo.txt &&
	git add b/c/d/foo.txt &&
	git commit -a -m "add dir" &&
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	svn_cmd up &&
	test -e b/c/d/foo.txt &&
	cd .. &&
	rm -rf b &&
	git commit -a -m "rm dir" &&
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	svn_cmd up &&
	test ! -e b &&
	cd ..
'

test_expect_success 'remove file' '
	touch foo.txt &&
	git add foo.txt &&
	git commit -a -m "add file" &&
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	svn_cmd up &&
	test -e foo.txt &&
	cd .. &&
	rm foo.txt &&
	git commit -a -m "rm file" &&
	git svn-push -v svn/master svn/master master
	cd svnco &&
	svn_cmd up &&
	test ! -e foo.txt &&
	cd ..
'

test_expect_success 'remove svn empty directories' '
	cd svnco &&
	svn_cmd mkdir empty &&
	svn_cmd commit -m "make empty" &&
	cd .. &&
	git svn-fetch -v &&
	git reset --hard svn/master &&
	test -e empty/.gitempty &&
	rm empty/.gitempty &&
	git commit -a -m "remove empty" &&
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	test "$(git clean -n -d | grep empty)" = "Would remove empty/" &&
	cd ..
'

test_expect_success '.git files' '
	mkdir h &&
	touch h/.githidden &&
	git add h/.githidden &&
	git commit -a -m "add h/.githidden" &&
	git svn-push -v svn/master svn/master master &&
	cd svnco &&
	svn_cmd up &&
	test -e h &&
	test ! -e h/.githidden &&
	cd ..
'

test_done

