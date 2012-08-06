#!/bin/sh

test_description='git svn-push branch'
. ./lib-git-svn-fetch.sh

function check_branched() {
	copyfrom_path="$1"
	copyfrom_rev="$2"
	test_must_fail svn_cmd log -r PREV --stop-on-copy &&
	test -n `svn_cmd log -1 -v --xml | grep copyfrom-path="/$copyfrom_path"` &&
	test -n `svn_cmd log -1 -v --xml | grep copyfrom-rev="$copyfrom_rev"`
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
	test_svn_subject BASE "init trunk" &&
	test_svn_author BASE committer &&
	test_file Trunk/file.txt "foo" &&
	cd ..
'

init_trunk_rev=2

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
	test_svn_subject BASE "init standalone" &&
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
	test_svn_subject BASE "Create Branches/CreateBranch" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../..
'

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
	test_svn_subject BASE "create/edit branch" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'create tag' '
	git checkout master &&
	git tag SimpleTag &&
	git svn-push -v refs/tags/SimpleTag $null_sha1 HEAD &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/SimpleTag &&
	test_file file.txt "foo" &&
	test_svn_subject BASE "Create Tags/SimpleTag" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'create annotated tag' '
	git checkout master &&
	git tag -m "annotate tag" AnnotatedTag &&
	git svn-push -v refs/tags/AnnotatedTag $null_sha1 AnnotatedTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/AnnotatedTag &&
	test_file file.txt "foo" &&
	test_svn_subject BASE "annotate tag" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../..
'

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
	test_svn_subject BASE "before replace" &&
	check_branched Trunk $init_trunk_rev &&
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
	test_svn_subject BASE "after replace" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../..
'

test_expect_success 'replace tag' '
	git checkout master &&
	git tag ReplaceTag &&
	git svn-push -v refs/tags/ReplaceTag $null_sha1 ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject BASE "Create Tags/ReplaceTag" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../.. &&
	git checkout -b temp master &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "replace tag" &&
	git tag -f ReplaceTag &&
	git svn-push -v ReplaceTag master ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject BASE "Create Tags/ReplaceTag" &&
	check_branched Trunk $init_trunk_rev &&
	cd ../../.. &&
	git tag -f -m "create replace tag" &&
	git svn-push -v ReplaceTag HEAD ReplaceTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/ReplaceTag &&
	test_svn_subject BASE "create replace tag" &&
	check_branched Trunk $init_trunk_rev &&
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
	test_svn_subject BASE "Create Branches/DeleteBranch" &&
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
	git svn-push -v refs/tags/DeleteTag $null_sha1 DeleteTag &&
	cd svnco &&
	svn_cmd up &&
	cd Tags/DeleteTag &&
	test_svn_subject BASE "Create Tags/DeleteTag" &&
	cd ../../.. &&
	git svn-push -v DeleteTag DeleteTag $null_sha1 &&
	git tag -d DeleteTag &&
	cd svnco &&
	svn_cmd up &&
	test ! -e Tags/DeleteTag &&
	cd ..
'

test_expect_success 'modify and create branch' '
	git checkout -b MCBranch1 master &&
	git svn-push -v refs/remotes/svn/MCBranch1 $null_sha1 MCBranch1 &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MCBranch1" &&
	echo "$null_sha1 `show_ref MCBranch2` refs/remotes/svn/MCBranch2" >> cmds.txt &&
	echo "`show_ref master` `show_ref HEAD` refs/remotes/svn/MCBranch1" >> cmds.txt &&
	git svn-push -v --stdin < cmds.txt &&
	rm -f cmds.txt &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/MCBranch1 &&
	test_svn_subject BASE "some modification on MCBranch1" &&
	cd ../MCBranch2 &&
	test_svn_subject BASE "Create Branches/MCBranch2" &&
	check_branched Branches/MCBranch1 PREV &&
	cd ../../..
'

test_expect_success 'modify and replace branch' '
	git checkout -b MRBranch1 master &&
	echo "change" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MRBranch1" &&
	git svn-push -v refs/remotes/svn/MRBranch1 $null_sha1 HEAD &&
	git checkout -b MRBranch2 master &&
	git svn-push -v refs/remotes/svn/MRBranch2 $null_sha1 HEAD &&
	echo "bar" > file2.txt &&
	git add file2.txt &&
	git commit -a -m "some modification on MRBranch2" &&
	echo "`show_ref MRBranch1` `show_ref MRBranch2` svn/MCBranch1" >> cmds.txt &&
	echo "`show_ref master` `show_ref MRBranch1` svn/MRBranch1" >> cmds.txt &&
	git svn-push -v --stdin < cmds.txt &&
	rm -f cmds.txt &&
	cd svnco &&
	svn_cmd up &&
	cd Branches/MRBranch1 &&
	test_svn_subject BASE "some modification on MRBranch2" &&
	test_must_fail svn_cmd log -r PREV --stop-on-copy &&
	cd ../MRBranch2 &&
	test_svn_subject BASE "some modification on MRBranch2" &&
	test_svn_subject PREV "Create Branches/MRBranch2" &&
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
	git svn-push -v refs/tags/TagOnDeleteBranch $null_sha1 TagOnDeleteBranch &&
	cd svnco &&
	svn_cmd up &&
	test ! -e Branches/DeleteBranch &&
	cd Tags/TagOnDeleteBranch &&
	test_svn_subject BASE "Create Tags/TagOnDeleteBranch" &&
	test_must_fail svn_cmd log -r PREV --stop-on-copy &&
	cd ../../.. &&
	git branch -D DeleteBranch
'

