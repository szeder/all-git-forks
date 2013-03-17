#!/bin/sh

test_description="git remote-svn fetch $svn_proto"
. ./lib-git-remote-svn.sh

test_expect_success 'mergeinfo' '
	echo "test" | git remote-svn--helper
'

test_expect_success 'fetch empty' '
	git fetch -v svn &&
	test_must_fail git checkout svn/master
'

test_expect_success 'init repo' '
	cd svnco &&
	svn_cmd mkdir empty-dir &&
	echo "some contents" > file.txt &&
	svn_cmd add file.txt &&
	svn_cmd ci -m "some commit" &&
	cd ..
'

export SVN_USERNAME=committer
export SVN_PASSWORD=pass
date=`svn_date HEAD svnco`

test_expect_success 'fetch repo' '
	#GIT_REMOTE_SVN_HELPER_PAUSE=1 git fetch -v svn &&
	#GIT_REMOTE_SVN_PAUSE=1 git fetch -v svn &&
	git fetch -v svn &&
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
	echo "666f6f0d0a6261720d0a" | xxd -r -p > crlf.txt &&
	svn_cmd add crlf.txt &&
	svn_cmd ci -m "crlf" &&
	cd .. &&
	echo "* text=auto" > .git/info/attributes &&
	git config remote.svn.eol crlf &&
	git config core.eol lf &&
	#GIT_REMOTE_SVN_HELPER_PAUSE=1 git fetch -v svn &&
	#GIT_REMOTE_SVN_PAUSE=1 git fetch -v svn &&
	git fetch -v svn &&
	git checkout svn/master &&
	echo "666f6f0a6261720a" | xxd -r -p > crlf_test.txt &&
	test "$(cat crlf.txt)" = "$(cat crlf_test.txt)" &&
	git config --unset remote.svn.eol
'

test_expect_success 'move file' '
	cd svnco &&
	seq 1 1000 > somefile &&
	svn_cmd add somefile &&
	svn_cmd ci -m "adding some file" &&
	svn_cmd mv somefile somefile2 &&
	svn_cmd ci -m "moving file" &&
	cd .. &&
	git fetch -v svn &&
	git checkout -f svn/master &&
	cmp somefile2 svnco/somefile2
'

test_expect_success 'move folder' '
	cd svnco &&
	svn_cmd mkdir folder &&
	cd folder &&
	seq 1 1000 > file1 &&
	seq 1000 2000 > file2 &&
	seq 3000 50000 > file3 &&
	svn_cmd add file1 file2 file3 &&
	cd .. &&
	svn_cmd ci -m "add some folder" &&
	svn_cmd mv folder folder2 &&
	svn_cmd ci -m "move folder" &&
	cd .. &&
	git fetch -v svn &&
	git checkout -f svn/master &&
	cmp folder2/file1 svnco/folder2/file1 &&
	cmp folder2/file2 svnco/folder2/file2 &&
	cmp folder2/file3 svnco/folder2/file3
'

final_sha1=`show_ref svn/master`

test_expect_success 'clone' '
	git clone -v -c core.attributesfile="$PWD/.git/info/attributes" -c svn.authors="$PWD/.git/svn-authors" svn::$svnurl gitco &&
	cd gitco &&
	test `show_ref master` = $final_sha1 &&
	cd ..
'

test_expect_success 'clone bare' '
	git clone --bare -v -c core.attributesfile="$PWD/.git/info/attributes" -c svn.authors="$PWD/.git/svn-authors" svn::$svnurl gitco2 &&
	cd gitco2 &&
	test `show_ref master` = $final_sha1 &&
	cd ..
'

test_done

