#!/bin/sh

test_description="git remote-svn fetch branch $svn_proto"
. ./lib-git-remote-svn.sh

test_expect_success 'setup branches' '
	git config --add remote.svn.map "Trunk:refs/heads/trunk" &&
	git config --add remote.svn.map "Branches/*:refs/heads/*" &&
	git config --add remote.svn.map "Tags/*:refs/tags/*" &&
	cd svnco &&
	svn_cmd mkdir Trunk &&
	svn_cmd mkdir Branches &&
	svn_cmd mkdir Tags &&
	touch Trunk/file.txt &&
	svn_cmd add Trunk/file.txt &&
	svn_cmd ci -m "init" &&
	svn_cmd up &&
	echo "other" >> Trunk/file.txt &&
	svn_cmd ci -m "trunk file" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	git fetch -v svn &&
	git checkout svn/trunk &&
	test_file file.txt "other"
'

test_expect_success 'copied branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/Branch &&
	svn_cmd ci -m "create branch" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	test_mergeinfo /Branches/Branch.3 "\"/Branches/Branch:3\n/Trunk:1-2\"" &&
	test `show_ref svn/trunk` = `show_ref svn/Branch`
'

test_expect_success 'copied and edited branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/CopiedBranch &&
	echo "more" >> Branches/CopiedBranch/file2.txt &&
	svn_cmd add Branches/CopiedBranch/file2.txt &&
	svn_cmd ci -m "create copied branch" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	git checkout svn/CopiedBranch &&
	test_mergeinfo /Branches/CopiedBranch.4 "\"/Branches/CopiedBranch:4\n/Trunk:1-3\"" &&
	test_file file.txt "other" &&
	test_file file2.txt "more" &&
	test_git_subject HEAD "create copied branch" &&
	test_git_subject HEAD~1 "trunk file" &&
	test_git_subject HEAD~2 "init" &&
	test `git log --pretty=oneline svn/trunk..svn/CopiedBranch | wc -l` -eq 1 &&
	merge_base svn/trunk svn/CopiedBranch
'

test_expect_success 'edited and copied branch' '
	cd svnco &&
	echo "more" >> Trunk/file2.txt &&
	svn_cmd add Trunk/file2.txt &&
	svn_cmd copy Trunk "Branches/Edit Copy Branch" &&
	svn_cmd ci -m "create edit copy branch" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	git checkout svn/Edit_Copy_Branch &&
	test_mergeinfo /Branches/Edit_Copy_Branch.5 "\"/Branches/Edit Copy Branch:5\n/Trunk:1-4\"" &&
	test_file file.txt "other" &&
	test_file file2.txt "more" &&
	test_git_subject HEAD "create edit copy branch" &&
	test `show_ref svn/trunk` = `show_ref svn/Edit_Copy_Branch`
'

# the copy commits shouldn't create git commits

test_expect_success 'copy, copy, copy' '
	cd svnco &&
	svn_cmd copy Trunk Branches/FastCopy2 &&
	svn_cmd copy Branches/FastCopy2 Branches/FastCopy1 &&
	svn_cmd copy Branches/FastCopy2 Branches/FastCopy3 &&
	svn_cmd ci -m "fast copy" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	test `show_ref svn/FastCopy1` = `show_ref svn/FastCopy2` &&
	test `show_ref svn/FastCopy1` = `show_ref svn/FastCopy3` &&
	test `show_ref svn/FastCopy1` = `show_ref svn/trunk` &&
	test_must_fail test_git_subject svn/FastCopy1 "fast copy"
'

# 'edit copy delete 1' shouldn't create a git commit

test_expect_success 'edit, copy, and delete' '
	cd svnco &&
	svn_cmd copy Trunk Branches/EditCopyDelete &&
	svn_cmd ci -m "edit copy delete 1" &&
	svn_cmd up &&
	echo "edit copy delete" >> Branches/EditCopyDelete/file3.txt &&
	svn_cmd add Branches/EditCopyDelete/file3.txt &&
	svn_cmd copy Branches/EditCopyDelete Branches/EditCopyDelete2 &&
	svn_cmd rm --force Branches/EditCopyDelete &&
	svn_cmd ci -m "edit copy delete 2" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	test_must_fail git checkout svn/EditCopyDelete &&
	git checkout svn/EditCopyDelete2 &&
	test_git_subject HEAD "edit copy delete 2" &&
	test_must_fail test_git_subject HEAD~1 "edit copy delete 1" &&
	test_file file3.txt "edit copy delete"
'

test_expect_success 'copy, edit, copy, and delete' '
	cd svnco &&
	svn_cmd copy Trunk Branches/CopyEditCopyDelete &&
	echo "copy edit copy delete" >> Branches/CopyEditCopyDelete/file4.txt &&
	svn_cmd add Branches/CopyEditCopyDelete/file4.txt &&
	svn_cmd copy Branches/CopyEditCopyDelete Branches/CopyEditCopyDelete2 &&
	svn_cmd rm --force Branches/CopyEditCopyDelete &&
	svn_cmd ci -m "copy edit copy delete" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	test_must_fail git checkout svn/CopyEditCopyDelete &&
	git checkout svn/CopyEditCopyDelete2 &&
	test_git_subject HEAD "copy edit copy delete" &&
	test_file file4.txt "copy edit copy delete"
'

test_expect_success 'non copied branch' '
	cd svnco &&
	svn_cmd mkdir Branches/NonCopiedBranch &&
	echo "non copied" >> Branches/NonCopiedBranch/file.txt &&
	svn_cmd add Branches/NonCopiedBranch/file.txt &&
	svn_cmd ci -m "create non-copied branch" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	git checkout svn/NonCopiedBranch &&
	test_file file.txt "non copied" &&
	test ! -e file2.txt &&
	test_git_subject HEAD "create non-copied branch" &&
	test `git log --pretty=oneline | wc -l` -eq 1 &&
	test_must_fail merge_base svn/trunk svn/NonCopiedBranch
'

test_expect_success 'removed branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/RemovedBranch &&
	svn_cmd ci -m "create branch" &&
	cd .. &&
	git fetch -v svn &&
	test `show_ref svn/RemovedBranch` = `show_ref svn/trunk` &&
	rev=`latest_revision` &&
	echo $rev &&
	cd svnco &&
	svn_cmd rm Branches/RemovedBranch &&
	svn_cmd ci -m "remove branch" &&
	cd .. &&
	delete_remote_refs &&
	git fetch -v svn &&
	test_must_fail git checkout svn/RemovedBranch &&
	cd svnco &&
	# Do the copy on the server as only svn >= 1.7 supports copying from a deleted working dir
	svn_cmd copy -m "copy branch" $svnurl/Branches/RemovedBranch@$rev $svnurl/Branches/RemovedBranch2 &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	test `show_ref svn/RemovedBranch2` = `show_ref svn/trunk` &&
	git checkout svn/RemovedBranch2 &&
	cd svnco &&
	svn_cmd copy Trunk Branches/RemovedBranch &&
	echo "foo" > Branches/RemovedBranch/newfile.txt &&
	svn_cmd add Branches/RemovedBranch/newfile.txt &&
	svn_cmd ci -m "create branch again" &&
	svn_cmd up &&
	cd .. &&
	git fetch -v svn &&
	latest=`latest_revision` &&
	# SVN 1.6 cant generate a replace via the cmd line client so we have to use a dump
	cat > svndump <<! &&
SVN-fs-dump-format-version: 2

Revision-number: $(($latest+1))
Prop-content-length: 75
Content-length: 75

K 7
svn:log
V 17
copy branch again
K 10
svn:author
V 9
committer
PROPS-END

Node-path: Branches/RemovedBranch2
Node-kind: dir
Node-action: replace
Node-copyfrom-rev: $rev
Node-copyfrom-path: Branches/RemovedBranch


!
	svnadmin load "$svnrepo" < svndump &&
	git fetch -v svn &&
	git checkout svn/RemovedBranch &&
	git checkout svn/RemovedBranch2 &&
	test `show_ref svn/RemovedBranch` != `show_ref svn/trunk` &&
	test `show_ref svn/RemovedBranch2` = `show_ref svn/trunk`
'

test_expect_success 'move branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/MovedBranch &&
	svn_cmd ci -m "create branch" &&
	cd .. &&
	git fetch -v svn &&
	git checkout svn/MovedBranch &&
	cd svnco &&
	svn_cmd mv Branches/MovedBranch Branches/MovedBranch2 &&
	svn_cmd ci -m "move branch" &&
	cd .. &&
	delete_remote_refs &&
	git fetch -v svn &&
	test_must_fail git checkout svn/MovedBranch &&
	git checkout svn/MovedBranch2 &&
	test `show_ref svn/MovedBranch2` = `show_ref svn/trunk`
'

test_expect_success 'tag' '
	cd svnco &&
	svn_cmd copy Trunk Tags/Tag &&
	svn_cmd ci -m "create tag" &&
	cd .. &&
	git fetch -t -v svn &&
	git checkout Tag &&
	test `show_tag Tag` = `show_ref svn/trunk` &&
	rev=`latest_revision` &&
	cd svnco &&
	svn_cmd rm Tags/Tag &&
	svn_cmd ci -m "remove tag" &&
	cd .. &&
	delete_remote_refs &&
	git fetch -t -v svn &&
	test_must_fail git checkout Tag &&
	svn_cmd cp -m "copy tag" $svnurl/Tags/Tag@$rev $svnurl/Branches/CopiedTag &&
	git fetch -v svn &&
	git checkout svn/CopiedTag &&
	test `show_ref svn/trunk` = `show_ref refs/remotes/svn/CopiedTag` &&
	cd svnco &&
	svn_cmd up &&
	svn_cmd copy Branches/CopiedTag Tags/Tag &&
	svn_cmd ci -m "create tag again" &&
	cd .. &&
	git fetch -t -v svn &&
	git checkout Tag &&
	test `show_tag Tag` = `show_ref svn/trunk` &&
	cd svnco &&
	svn_cmd copy Tags/Tag Tags/Tag2 &&
	svn_cmd ci -m "create 2nd tag" &&
	cd .. &&
	rev=`latest_revision` &&
	# SVN 1.6 cant generate a replace via the cmd line client so we have to use a dump
	cat > svndump <<! &&
SVN-fs-dump-format-version: 2

Revision-number: $(($rev+1))
Prop-content-length: 79
Content-length: 79

K 7
svn:log
V 21
recreate tag from 2nd
K 10
svn:author
V 9
committer
PROPS-END

Node-path: Tags/Tag
Node-kind: dir
Node-action: replace
Node-copyfrom-rev: $rev
Node-copyfrom-path: Tags/Tag2

!
	svnadmin load "$svnrepo" < svndump &&
	git fetch -t -v svn &&
	test `show_tag Tag` = `show_tag Tag2` &&
	test `show_tag Tag` = `show_ref svn/trunk`
'

test_done

