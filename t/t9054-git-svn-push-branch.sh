#!/bin/sh

test_description='git svn-push branch'
. ./lib-git-svn-fetch.sh

function check_branched() {
	copyfrom_path="$1"
	copyfrom_rev="$2"
	echo check_branched $1 $2
	svn_cmd log --stop-on-copy -v --xml | grep copyfrom-path=\"/$copyfrom_path\" &&
	svn_cmd log --stop-on-copy -v --xml | grep copyfrom-rev=\"$copyfrom_rev\"
}

test_expect_success 'setup branches' '
	git config svn.trunk Trunk &&
	git config svn.branches Branches &&
	git config svn.tags Tags &&
	git config svn.trunkref trunk &&
	cd svnco &&
	svn_cmd mkdir Branches &&
	svn_cmd mkdir Tags &&
	svn_cmd ci -m "svn init" &&
	cd ..
'

test_expect_success 'init trunk' '
	echo "foo" > file.txt &&
	git add file.txt &&
	git commit -a -m "init trunk" &&
	git svn-push -v refs/remotes/svn/trunk $null_sha1 master &&
	test `show_ref svn/trunk` == `show_ref master` &&
	cd svnco &&
	svn_cmd up &&
	test -d Trunk &&
	test_svn_subject "init trunk" &&
	test_svn_author committer &&
	test_file Trunk/file.txt "foo" &&
	cd ..
'

function svn_head() {
	wd=`pwd` &&
	cd svnco &&
	cd "$1" &&
	svn info | grep Revision | sed -e 's/Revision: *//g' &&
	cd "$wd"
}

init_trunk_rev=`svn_head Trunk`
init_trunk_path=Trunk

test_expect_success 'create standalone branch' '
	git symbolic-ref HEAD refs/heads/standalone &&
	git rm -r --cached . &&
	echo "bar" > file.txt &&
	git add file.txt &&
	git commit -a -m "init standalone" &&
	git svn-push -v refs/remotes/svn/standalone $null_sha1 standalone &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/standalone &&
	test_file file.txt "bar" &&
	test_svn_subject "init standalone" &&
	test_must_fail svn_cmd log PREV &&
	cd ../../..
'

test_expect_success 'create branch' '
	git checkout -b CreateBranch master &&
	git svn-push -v refs/remotes/svn/CreateBranch $null_sha1 CreateBranch &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/CreateBranch &&
	test_file file.txt "foo" &&
	test_svn_subject "Create Branches/CreateBranch" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

init_trunk_rev=`svn_head Branches/CreateBranch`
init_trunk_path=Branches/CreateBranch

test_expect_success 'create and edit branch' '
	git checkout -b CreateEditBranch master &&
	echo "foo2" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "create/edit branch" &&
	git svn-push -v refs/remotes/svn/CreateEditBranch $null_sha1 CreateEditBranch &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/CreateEditBranch &&
	test_file file.txt "foo" &&
	test_file file2.txt "foo2" &&
	test_svn_subject "create/edit branch" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'create tag' '
	git checkout master &&
	git tag SimpleTag &&
	git svn-push -v refs/tags/svn/SimpleTag $null_sha1 HEAD &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/SimpleTag &&
	test_file file.txt "foo" &&
	test_svn_subject "Create Tags/SimpleTag" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

init_trunk_rev=`svn_head Tags/SimpleTag`
init_trunk_path=Tags/SimpleTag

test_expect_success 'create annotated tag' '
	git checkout master &&
	git tag -m "annotate tag" AnnotatedTag &&
	git svn-push -v refs/tags/svn/AnnotatedTag $null_sha1 AnnotatedTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/AnnotatedTag &&
	test_file file.txt "foo" &&
	test_svn_subject "annotate tag" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

init_trunk_rev=`svn_head Tags/AnnotatedTag`
init_trunk_path=Tags/AnnotatedTag

test_expect_success 'replace branch' '
	git checkout -b ReplaceBranch master &&
	echo "before replace" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "before replace" &&
	before_sha1=`show_ref HEAD` &&
	git svn-push -v refs/remotes/svn/ReplaceBranch $null_sha1 ReplaceBranch &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/ReplaceBranch &&
	test_file file2.txt "before replace" &&
	test_svn_subject "before replace" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../.. &&
	git checkout master &&
	git branch -D ReplaceBranch &&
	git checkout -b ReplaceBranch master &&
	echo "after replace" > file3.txt &&
	git add file3.txt &&
	git commit -a -m "after replace" &&
	git svn-push -v svn/ReplaceBranch $before_sha1 ReplaceBranch &&
	cd svnco/Branches/ReplaceBranch &&
	svn_cmd up &&
	test ! -e file2.txt &&
	test_file file3.txt "after replace" &&
	test_svn_subject "after replace" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'replace tag' '
	git checkout -b temp master &&
	echo "foo" > file3.txt &&
	git add file3.txt &&
	git commit -a -m "before replace tag" &&
	old_sha1=`show_ref HEAD` &&
	git tag ReplaceTag &&
	git svn-push -v refs/tags/svn/ReplaceTag $null_sha1 ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject "before replace tag" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../.. &&
	git reset --hard master &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "after replace tag" &&
	git tag -f ReplaceTag &&
	git svn-push -v svn/ReplaceTag $old_sha1 ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject "after replace tag" &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../.. &&
	echo "bar2" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "dummy commit" &&
	git tag -f -m "create replace tag" ReplaceTag &&
	git svn-push -v svn/ReplaceTag HEAD~ ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject "create replace tag" &&
	test_svn_subject "after replace tag" PREV &&
	cd ../../.. &&
	git checkout master &&
	git branch -D temp
'

test_expect_success 'delete branch' '
	git checkout -b DeleteBranch master &&
	git svn-push -v refs/remotes/svn/DeleteBranch $null_sha1 DeleteBranch &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/DeleteBranch &&
	test_svn_subject "Create Branches/DeleteBranch" &&
	cd ../../.. &&
	git checkout master &&
	git svn-push -v svn/DeleteBranch DeleteBranch $null_sha1 &&
	git branch -D DeleteBranch &&
	cd svnco &&
	svn_cmd up &&
	test ! -e Branches/DeleteBranch &&
	cd ..
'

test_expect_success 'delete tag' '
	git checkout master &&
	git tag DeleteTag &&
	git svn-push -v refs/tags/svn/DeleteTag $null_sha1 DeleteTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/DeleteTag &&
	test_svn_subject "Create Tags/DeleteTag" &&
	cd ../../.. &&
	git svn-push -v svn/DeleteTag DeleteTag $null_sha1 &&
	git tag -d DeleteTag &&
	cd svnco &&
	svn_cmd up &&
	test ! -e Tags/DeleteTag &&
	cd ..
'

test_expect_success 'modify and create branch' '
	git checkout -b MCBranch1 master &&
	git svn-push -v refs/remotes/svn/MCBranch1 $null_sha1 MCBranch1 &&
	cd svnco &&
	svn_cmd up &&
	cd .. &&
	init_trunk_rev=`svn_head Branches/MCBranch1`
	init_trunk_path=Branches/MCBranch1 &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MCBranch1" &&
	echo "$null_sha1 `show_ref HEAD` refs/remotes/svn/MCBranch2" >> cmds.txt &&
	echo "`show_ref master` `show_ref HEAD` refs/remotes/svn/MCBranch1" >> cmds.txt &&
	git svn-push -v --stdin < cmds.txt &&
	rm -f cmds.txt &&
	before_rev=`svn_head Branches/MCBranch1` &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/MCBranch1 &&
	test_svn_subject "some modification on MCBranch1" &&
	cd ../MCBranch2 &&
	test_svn_subject "Create Branches/MCBranch2" &&
	check_branched Branches/MCBranch1 $(($before_rev+1)) &&
	cd ../../..
'
test_expect_success 'modify and replace branch' '
	git checkout -b MRBranch1 master &&
	echo "change" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MRBranch1" &&
	git svn-push -v refs/remotes/svn/MRBranch1 $null_sha1 MRBranch1 &&
	git svn-push -v refs/remotes/svn/MRBranch2 $null_sha1 master &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/MRBranch1 &&
	test_svn_subject "some modification on MRBranch1" &&
	cd ../MRBranch2 &&
	test_svn_subject "Create Branches/MRBranch2" &&
	cd ../../.. &&
	before_rev=`svn_head Branches/MRBranch2` &&
	git checkout -b MRBranch2 master &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MRBranch2" &&
	echo "`show_ref MRBranch1` `show_ref MRBranch2` svn/MRBranch1" > cmds.txt &&
	echo "`show_ref master` `show_ref MRBranch2` svn/MRBranch2" >> cmds.txt &&
	git svn-push -v --stdin < cmds.txt &&
	rm -f cmds.txt &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/MRBranch1 &&
	test_svn_subject "Create Branches/MRBranch1" &&
	check_branched Branches/MRBranch2 $(($before_rev+1)) &&
	cd ../MRBranch2 &&
	test_svn_subject "some modification on MRBranch2" &&
	test_svn_subject "Create Branches/MRBranch2" PREV &&
	check_branched $init_trunk_path $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'tag deleted branch' '
	git checkout -b DeleteBranch master &&
	echo "foo" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "commit on deleted branch" &&
	git svn-push -v refs/remotes/svn/DeleteBranch $null_sha1 HEAD &&
	git tag TagOnDeleteBranch &&
	git checkout master &&
	git svn-push -v svn/DeleteBranch DeleteBranch $null_sha1 &&
	git svn-push -v refs/tags/svn/TagOnDeleteBranch $null_sha1 TagOnDeleteBranch &&
	cd svnco &&
	svn_cmd up &&
	test ! -e Branches/DeleteBranch &&
	cd Tags/TagOnDeleteBranch &&
	test_svn_subject "Create Tags/TagOnDeleteBranch" &&
	test_must_fail svn_cmd log -r PREV --stop-on-copy &&
	cd ../../.. &&
	git branch -D DeleteBranch
'

test_expect_success 'push left merge' '
	git checkout -b LeftLeft master &&
	echo "left" > left.txt &&
	git add left.txt &&
	git commit -m "left" &&
	git svn-push -v refs/remotes/svn/LeftMerged $null_sha1 HEAD &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/LeftMerged &&
	prev_path=`svn_cmd log --stop-on-copy -v --xml | grep copyfrom-path` &&
	prev_rev=`svn_cmd log --stop-on-copy -v --xml | grep copyfrom-rev` &&
	cd ../../.. &&
	git checkout -b LeftRight master &&
	echo "right" > right.txt &&
	git add right.txt &&
	git commit -m "right" &&
	git checkout LeftLeft &&
	git merge --no-ff "merge commit" HEAD LeftRight &&
	git svn-push -v svn/LeftMerged svn/LeftMerged LeftLeft &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/LeftMerged &&
	test_file left.txt "left" &&
	test_file right.txt "right" &&
	test_svn_subject "merge commit" &&
	test_svn_subject "left" PREV &&
	svn_cmd log --stop-on-copy -v --xml | grep $prev_path &&
	svn_cmd log --stop-on-copy -v --xml | grep $prev_rev &&
	cd ../../..
'

test_expect_success 'push right merge' '
	git checkout -b RightLeft master &&
	echo "left" > left.txt &&
	git add left.txt &&
	git commit -m "left" &&
	git svn-push -v refs/remotes/svn/RightMerged $null_sha1 HEAD &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/RightMerged &&
	prev_path=`svn_cmd log --stop-on-copy -v --xml | grep copyfrom-path` &&
	prev_rev=`svn_cmd log --stop-on-copy -v --xml | grep copyfrom-rev` &&
	cd ../../.. &&
	git checkout -b RightRight master &&
	echo "right" > right.txt &&
	git add right.txt &&
	git commit -m "right" &&
	git merge --no-ff "merge commit" HEAD RightLeft &&
	git svn-push -v svn/RightMerged svn/RightMerged RightRight &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/RightMerged &&
	test_svn_subject "merge commit" &&
	test_svn_subject "left" PREV &&
	svn_cmd log --stop-on-copy -v --xml | grep $prev_path &&
	svn_cmd log --stop-on-copy -v --xml | grep $prev_rev &&
	cd ../../..
'

test_done
