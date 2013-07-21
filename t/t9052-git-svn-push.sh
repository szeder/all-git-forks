#!/bin/sh

test_description="git remote-svn push $svn_proto"
. ./lib-git-remote-svn.sh

test_expect_success 'init push' '
	echo "#!/bin/sh" > askpass &&
	echo "echo pass" >> askpass &&
	chmod +x askpass &&
	git config core.askpass "$PWD/askpass" &&
	git config "credential.$svnurl.username" committer &&
	echo "foo" > file.txt &&
	git add file.txt &&
	git commit -a -m "initial commit" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test_svn_subject 1 "initial commit" &&
	test_svn_author committer &&
	test_file file.txt "foo" &&
	cd .. &&
	git checkout refs/svn/$uuid.1 &&
	test_file file.txt "foo"
'

test_expect_success 'multiple commits' '
	git checkout master &&
	echo "bar" >> file.txt &&
	git commit -a -m "second commit" &&
	mkdir a &&
	echo "fefifofum" >> a/test &&
	git add a/test &&
	git commit -a -m "third commit" &&
	git push -v svn master &&
	echo foo > file_test.txt &&
	echo bar >> file_test.txt &&
	git checkout refs/svn/$uuid.1 &&
	cmp file.txt file_test.txt &&
	cd svnco &&
	svn_cmd up -r 2 &&
	test_svn_subject 1 "second commit" &&
	test_svn_author committer &&
	cmp file.txt ../file_test.txt &&
	test ! -e a &&
	svn_cmd up -r 3 &&
	test_svn_subject 1 "third commit" &&
	test_svn_author committer &&
	test_file a/test "fefifofum" &&
	cd ..
'

test_expect_success 'remove git empty directories' '
	git checkout master &&
	mkdir -p b/c/d &&
	touch b/c/d/foo.txt &&
	git add b/c/d/foo.txt &&
	git commit -a -m "add dir" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test -e b/c/d/foo.txt &&
	cd .. &&
	rm -rf b &&
	git commit -a -m "rm dir" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test ! -e b &&
	cd ..
'

test_expect_success 'remove file' '
	touch foo.txt &&
	git add foo.txt &&
	git commit -a -m "add file" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test -e foo.txt &&
	cd .. &&
	rm foo.txt &&
	git commit -a -m "rm file" &&
	git push -v svn master &&
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
	git fetch -v svn &&
	git reset --hard svn/master &&
	test -e empty/.gitempty &&
	rm empty/.gitempty &&
	git commit -a -m "remove empty" &&
	git push -v svn master &&
	cd svnco &&
	test "$(git clean -n -d | grep empty)" = "Would remove empty/" &&
	cd ..
'

test_expect_success '.git files' '
	mkdir h &&
	touch h/.githidden &&
	git add h/.githidden &&
	git commit -a -m "add h/.githidden" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test -e h &&
	test ! -e h/.githidden &&
	cd ..
'

test_expect_success 'modify file' '
	echo "foo" > file.txt &&
	git add file.txt &&
	git commit -a -m "edit1" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test_svn_subject 1 "edit1" &&
	test_file file.txt "foo" &&
	cd .. &&
	echo "bar" > file.txt &&
	git commit -a -m "edit2" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test_svn_subject 1 "edit2" &&
	test_file file.txt "bar" &&
	cd ..
'

test_expect_success 'big file' '
	seq 1 50 > small.txt &&
	seq 1 100000 > big.txt &&
	git add small.txt big.txt &&
	git commit -a -m "big file" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	cmp small.txt ../small.txt &&
	cmp big.txt ../big.txt &&
	cd ..
'

test_expect_success 'gitpath' '
	git config remote.svn.gitpath svn &&
	mkdir svn &&
	echo foo > svn/file.txt &&
	git add svn/file.txt &&
	git commit -a -m "add svn/file.txt" &&
	git push -v svn master &&
	cd svnco &&
	svn_cmd up &&
	test_file file.txt "foo" &&
	cd ..
'

test_done

