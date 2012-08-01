#!/bin/sh

test_description='git svn-fetch branch'
. ./lib-git-svn-fetch.sh

test_expect_success 'setup branches' '
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
	git config svn.trunk Trunk &&
	git config svn.branches Branches &&
	git config svn.tags Tags &&
	git config svn.url $svnurl &&
	git svn-fetch -v
'

test_expect_success 'copied branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/Branch &&
	svn_cmd ci -m "create branch" &&
	svn_cmd up &&
	cd .. &&
	git svn-fetch -v &&
	test `show_ref svn/trunk` == `show_ref svn/Branch`
'

test_expect_success 'copied and edited branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/CopiedBranch &&
	echo "more" >> Branches/CopiedBranch/file2.txt &&
	svn_cmd add Branches/CopiedBranch/file2.txt &&
	svn_cmd ci -m "create copied branch" &&
	svn_cmd up &&
	cd .. &&
	git svn-fetch -v &&
	git checkout svn/CopiedBranch &&
	test_file file.txt "other" &&
	test_file file2.txt "more" &&
	test_subject HEAD "create copied branch" &&
	test_subject HEAD~1 "trunk file" &&
	test_subject HEAD~2 "init" &&
	test `git log --pretty=oneline svn/trunk..svn/CopiedBranch | wc -l` -eq 1 &&
	merge_base svn/trunk svn/CopiedBranch
'

test_expect_success 'edited and copied branch' '
	cd svnco &&
	echo "more" >> Trunk/file2.txt &&
	svn_cmd add Trunk/file2.txt &&
	svn_cmd copy Trunk Branches/EditCopyBranch &&
	svn_cmd ci -m "create edit copy branch" &&
	svn_cmd up &&
	cd .. &&
	git svn-fetch -v &&
	git checkout svn/EditCopyBranch &&
	test_file file.txt "other" &&
	test_file file2.txt "more" &&
	test_subject HEAD "create edit copy branch" &&
	test `show_ref svn/trunk` == `show_ref svn/EditCopyBranch`
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
	git svn-fetch -v &&
	test `show_ref svn/FastCopy1` == `show_ref svn/FastCopy2` &&
	test `show_ref svn/FastCopy1` == `show_ref svn/FastCopy3` &&
	test `show_ref svn/FastCopy1` == `show_ref svn/trunk` &&
	test_must_fail test_subject svn/FastCopy1 "fast copy"
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
	git svn-fetch -v &&
	test_must_fail git checkout svn/EditCopyDelete &&
	git checkout svn/EditCopyDelete2 &&
	test_subject HEAD "edit copy delete 2" &&
	test_must_fail test_subject HEAD~1 "edit copy delete 1" &&
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
	git svn-fetch -v &&
	test_must_fail git checkout svn/CopyEditCopyDelete &&
	git checkout svn/CopyEditCopyDelete2 &&
	test_subject HEAD "copy edit copy delete" &&
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
	git svn-fetch -v &&
	git checkout svn/NonCopiedBranch &&
	test_file file.txt "non copied" &&
	test ! -e file2.txt &&
	test_subject HEAD "create non-copied branch" &&
	test `git log --pretty=oneline | wc -l` -eq 1 &&
	test_must_fail merge_base svn/trunk svn/NonCopiedBranch
'

test_expect_success 'removed branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/RemovedBranch &&
	svn_cmd ci -m "create branch" &&
	cd .. &&
	git svn-fetch -v &&
	test `show_ref svn/RemovedBranch` == `show_ref svn/trunk` &&
	rev=$(cat .git/svn-latest) &&
	cd svnco &&
	svn_cmd rm Branches/RemovedBranch &&
	svn_cmd ci -m "remove branch" &&
	cd .. &&
	git svn-fetch -v &&
	test_must_fail git checkout svn/RemovedBranch &&
	cd svnco &&
	echo $rev &&
	svn_cmd copy Branches/RemovedBranch@$rev Branches/RemovedBranch2 &&
	svn_cmd ci -m "copy branch" &&
	svn_cmd up &&
	cd .. &&
	git svn-fetch -v &&
	test `show_ref svn/RemovedBranch2` == `show_ref svn/trunk` &&
	git checkout svn/RemovedBranch2 &&
	cd svnco &&
	svn_cmd copy Trunk Branches/RemovedBranch &&
	echo "foo" > Branches/RemovedBranch/newfile.txt &&
	svn_cmd add Branches/RemovedBranch/newfile.txt &&
	svn_cmd ci -m "create branch again" &&
	svn_cmd up &&
	svn_cmd rm Branches/RemovedBranch2 &&
	svn_cmd copy Branches/RemovedBranch@$rev Branches/RemovedBranch2 &&
	svn_cmd ci -m "copy branch again" &&
	svn_cmd up &&
	cd .. &&
	git svn-fetch -v &&
	git checkout svn/RemovedBranch &&
	git checkout svn/RemovedBranch2 &&
	test `show_ref svn/RemovedBranch` != `show_ref svn/trunk` &&
	test `show_ref svn/RemovedBranch2` == `show_ref svn/trunk`
'

test_expect_success 'move branch' '
	cd svnco &&
	svn_cmd copy Trunk Branches/MovedBranch &&
	svn_cmd ci -m "create branch" &&
	cd .. &&
	git svn-fetch -v &&
	git checkout svn/MovedBranch &&
	cd svnco &&
	svn_cmd mv Branches/MovedBranch Branches/MovedBranch2 &&
	svn_cmd ci -m "move branch" &&
	cd .. &&
	git svn-fetch -v &&
	test_must_fail git checkout svn/MovedBranch &&
	git checkout svn/MovedBranch2 &&
	test `show_ref svn/MovedBranch2` == `show_ref svn/trunk`
'

test_expect_success 'tag' '
	cd svnco &&
	svn_cmd copy Trunk Tags/Tag &&
	svn_cmd ci -m "create tag" &&
	cd .. &&
	git svn-fetch -v &&
	git checkout Tag &&
	test `show_tag Tag` == `show_ref svn/trunk` &&
	rev=$(cat .git/svn-latest) &&
	cd svnco &&
	svn_cmd rm Tags/Tag &&
	svn_cmd ci -m "remove tag" &&
	cd .. &&
	git svn-fetch -v &&
	test_must_fail git checkout Tag &&
	cd svnco &&
	svn_cmd copy Tags/Tag@$rev Branches/CopiedTag &&
	svn_cmd ci -m "copy tag" &&
	cd .. &&
	git svn-fetch -v &&
	git checkout svn/CopiedTag &&
	test `show_ref svn/trunk` == `show_ref svn/CopiedTag` &&
	cd svnco &&
	svn_cmd copy Branches/CopiedTag Tags/Tag &&
	svn_cmd ci -m "create tag again" &&
	cd .. &&
	git svn-fetch -v &&
	git checkout Tag &&
	test `show_tag Tag` == `show_ref svn/trunk` &&
	cd svnco &&
	svn_cmd copy Tags/Tag Tags/Tag2 &&
	svn_cmd ci -m "create 2nd tag" &&
	svn_cmd rm Tags/Tag &&
	svn_cmd copy Tags/Tag2 Tags/Tag &&
	svn_cmd ci -m "recreate tag from 2nd" &&
	cd .. &&
	git svn-fetch -v &&
	test `show_tag Tag` == `show_tag Tag2` &&
	test `show_tag Tag` == `show_ref svn/trunk`

'

test_done

