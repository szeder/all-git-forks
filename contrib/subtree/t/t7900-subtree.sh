#!/bin/sh
#
# Copyright (c) 2012 Avery Pennaraum
#
test_description='Basic porcelain support for subtrees

This test verifies the basic operation of the merge, push, pull, add,
split, push-all, pull-all and list subcommands of git subtree.
'

export TEST_DIRECTORY=$(pwd)/../../../t

. ../../../t/test-lib.sh

create()
{
	echo "$1" >"$1"
	git add "$1"
}


check_equal()
{
	test_debug 'echo'
	test_debug "echo \"check a:\" \"{$1}\""
	test_debug "echo \"      b:\" \"{$2}\""
	if [ "$1" = "$2" ]; then
		return 0
	else
		return 1
	fi
}

fixnl()
{
	t=""
	while read x; do
		t="$t$x "
	done
	echo $t
}

multiline()
{
	while read x; do
		set -- $x
		for d in "$@"; do
			echo "$d"
		done
	done
}

undo()
{
	git reset --hard HEAD~
}

last_commit_message()
{
	git log --pretty=format:%s -1
}

last_commit_id()
{
    git log --format="%H" -n 1
}

test_expect_success 'init subproj' '
        test_create_repo subproj
'

# To the subproject!
cd subproj

test_expect_success 'add sub1' '
        create sub1 &&
        git commit -m "sub1" &&
        git branch sub1 &&
        git branch -m master subproj
'

# Save this hash for testing later.

subdir_hash=`git rev-parse HEAD`

test_expect_success 'add sub2' '
        create sub2 &&
        git commit -m "sub2" &&
        git branch sub2
'

test_expect_success 'add sub3' '
        create sub3 &&
        git commit -m "sub3" &&
        git branch sub3
'

# Back to mainline
cd ..

test_expect_success 'add main4' '
        create main4 &&
        git commit -m "main4" &&
        git branch -m master mainline &&
        git branch subdir
'

test_expect_success 'fetch subproj history' '
        git fetch ./subproj sub1 &&
        git branch sub1 FETCH_HEAD
'

test_expect_success 'no subtree exists in main tree' '
        test_must_fail git subtree merge --prefix=subdir sub1
'

test_expect_success 'no pull from non-existant subtree' '
        test_must_fail git subtree pull --prefix=subdir ./subproj sub1
'

test_expect_success 'check if --message works for add' '
        git subtree add --prefix=subdir --message="Added subproject" sub1 &&
        check_equal ''"$(last_commit_message)"'' "Added subproject" &&
        undo
'

test_expect_success 'check if --message works as -m and --prefix as -P' '
        git subtree add -P subdir -m "Added subproject using git subtree" sub1 &&
        check_equal ''"$(last_commit_message)"'' "Added subproject using git subtree" &&
        undo
'

test_expect_success 'check if --message works with squash too' '
        git subtree add -P subdir -m "Added subproject with squash" --squash sub1 &&
        check_equal ''"$(last_commit_message)"'' "Added subproject with squash" &&
        undo
'

test_expect_success 'add subproj to mainline' '
        git subtree add --prefix=subdir/ FETCH_HEAD &&
        check_equal ''"$(last_commit_message)"'' "Add '"'subdir/'"' from commit '"'"'''"$(git rev-parse sub1)"'''"'"'"
'

# this shouldn't actually do anything, since FETCH_HEAD is already a parent
test_expect_success 'merge fetched subproj' '
        git merge -m "merge -s -ours" -s ours FETCH_HEAD
'

test_expect_success 'add main-sub5' '
        create subdir/main-sub5 &&
        git commit -m "main-sub5"
'

test_expect_success 'add main6' '
        create main6 &&
        git commit -m "main6 boring"
'

test_expect_success 'add main-sub7' '
        create subdir/main-sub7 &&
        git commit -m "main-sub7"
'

test_expect_success 'fetch new subproj history' '
        git fetch ./subproj sub2 &&
        git branch sub2 FETCH_HEAD
'

test_expect_success 'check if --message works for merge' '
        git subtree merge --prefix=subdir -m "Merged changes from subproject" sub2 &&
        check_equal ''"$(last_commit_message)"'' "Merged changes from subproject" &&
        undo
'

test_expect_success 'check if --message for merge works with squash too' '
        git subtree merge --prefix subdir -m "Merged changes from subproject using squash" --squash sub2 &&
        check_equal ''"$(last_commit_message)"'' "Merged changes from subproject using squash" &&
        undo
'

test_expect_success 'merge new subproj history into subdir' '
        git subtree merge --prefix=subdir FETCH_HEAD &&
        git branch pre-split &&
        check_equal ''"$(last_commit_message)"'' "Merge commit '"'"'"$(git rev-parse sub2)"'"'"' into mainline"
'

test_expect_success 'Check that prefix argument is required for split' '
        echo "You must provide the --prefix option." > expected &&
        test_must_fail git subtree split > actual 2>&1 &&
	test_debug "printf '"'"'expected: '"'"'" &&
        test_debug "cat expected" &&
	test_debug "printf '"'"'actual: '"'"'" &&
        test_debug "cat actual" &&
        test_cmp expected actual &&
        rm -f expected actual
'

test_expect_success 'Check that the <prefix> exists for a split' '
        echo "'"'"'non-existent-directory'"'"'" does not exist\; use "'"'"'git subtree add'"'"'" > expected &&
        test_must_fail git subtree split --prefix=non-existent-directory > actual 2>&1 &&
	test_debug "printf '"'"'expected: '"'"'" &&
        test_debug "cat expected" &&
	test_debug "printf '"'"'actual: '"'"'" &&
        test_debug "cat actual" &&
        test_cmp expected actual
#        rm -f expected actual
'

test_expect_success 'check if --message works for split+rejoin' '
        spl1=''"$(git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --message "Split & rejoin" --rejoin)"'' &&
        git branch spl1 "$spl1" &&
        check_equal ''"$(last_commit_message)"'' "Split & rejoin" &&
        undo
'

test_expect_success 'check split with --branch' '
	spl1=$(git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --message "Split & rejoin" --rejoin) &&
	undo &&
	git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --branch splitbr1 &&
	check_equal ''"$(git rev-parse splitbr1)"'' "$spl1"
'

test_expect_success 'check hash of split' '
	spl1=$(git subtree split --prefix subdir) &&
	undo &&
	git subtree split --prefix subdir --branch splitbr1test &&
	check_equal ''"$(git rev-parse splitbr1test)"'' "$spl1"
	git checkout splitbr1test &&
	new_hash=$(git rev-parse HEAD~2) &&
	git checkout mainline &&
	check_equal ''"$new_hash"'' "$subdir_hash"
'

test_expect_success 'check split with --branch for an existing branch' '
        spl1=''"$(git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --message "Split & rejoin" --rejoin)"'' &&
        undo &&
        git branch splitbr2 sub1 &&
        git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --branch splitbr2 &&
        check_equal ''"$(git rev-parse splitbr2)"'' "$spl1"
'

test_expect_success 'check split with --branch for an incompatible branch' '
        test_must_fail git subtree split --prefix subdir --onto FETCH_HEAD --branch subdir
'

test_expect_success 'check split+rejoin' '
        spl1=''"$(git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --message "Split & rejoin" --rejoin)"'' &&
        undo &&
        git subtree split --annotate='"'*'"' --prefix subdir --onto FETCH_HEAD --rejoin &&
        check_equal ''"$(last_commit_message)"'' "Split '"'"'subdir/'"'"' into commit '"'"'"$spl1"'"'"'"
'

test_expect_success 'add main-sub8' '
        create subdir/main-sub8 &&
        git commit -m "main-sub8"
'

# To the subproject!
cd ./subproj

test_expect_success 'merge split into subproj' '
        git fetch .. spl1 &&
        git branch spl1 FETCH_HEAD &&
        git merge FETCH_HEAD
'

test_expect_success 'add sub9' '
        create sub9 &&
        git commit -m "sub9"
'

# Back to mainline
cd ..

test_expect_success 'split for sub8' '
        split2=''"$(git subtree split --annotate='"'*'"' --prefix subdir/ --rejoin)"''
        git branch split2 "$split2"
'

test_expect_success 'add main-sub10' '
        create subdir/main-sub10 &&
        git commit -m "main-sub10"
'

test_expect_success 'split for sub10' '
        spl3=''"$(git subtree split --annotate='"'*'"' --prefix subdir --rejoin)"'' &&
        git branch spl3 "$spl3"
'

# To the subproject!
cd ./subproj

test_expect_success 'merge split into subproj' '
        git fetch .. spl3 &&
        git branch spl3 FETCH_HEAD &&
        git merge FETCH_HEAD &&
        git branch subproj-merge-spl3
'

chkm="main4 main6"
chkms="main-sub10 main-sub5 main-sub7 main-sub8"
chkms_sub=$(echo $chkms | multiline | sed 's,^,subdir/,' | fixnl)
chks="sub1 sub2 sub3 sub9"
chks_sub=$(echo $chks | multiline | sed 's,^,subdir/,' | fixnl)

test_expect_success 'make sure exactly the right set of files ends up in the subproj' '
        subfiles=''"$(git ls-files | fixnl)"'' &&
        check_equal "$subfiles" "$chkms $chks"
'

test_expect_success 'make sure the subproj history *only* contains commits that affect the subdir' '
        allchanges=''"$(git log --name-only --pretty=format:'"''"' | sort | fixnl)"'' &&
        check_equal "$allchanges" "$chkms $chks"
'

# Back to mainline
cd ..

test_expect_success 'pull from subproj' '
        git fetch ./subproj subproj-merge-spl3 &&
        git branch subproj-merge-spl3 FETCH_HEAD &&
        git subtree pull --prefix=subdir ./subproj subproj-merge-spl3
'

test_expect_success 'make sure exactly the right set of files ends up in the mainline' '
        mainfiles=''"$(git ls-files | fixnl)"'' &&
        check_equal "$mainfiles" "$chkm $chkms_sub $chks_sub"
'

test_expect_success 'make sure each filename changed exactly once in the entire history' '
        # main-sub?? and /subdir/main-sub?? both change, because those are the
        # changes that were split into their own history.  And subdir/sub?? never
        # change, since they were *only* changed in the subtree branch.
        allchanges=''"$(git log --name-only --pretty=format:'"''"' | sort | fixnl)"'' &&
        check_equal "$allchanges" ''"$(echo $chkms $chkm $chks $chkms_sub | multiline | sort | fixnl)"''
'

test_expect_success 'make sure the --rejoin commits never make it into subproj' '
        check_equal ''"$(git log --pretty=format:'"'%s'"' HEAD^2 | grep -i split)"'' ""
'

test_expect_success 'make sure no "git subtree" tagged commits make it into subproj' '
        # They are meaningless to subproj since one side of the merge refers to the mainline
        check_equal ''"$(git log --pretty=format:'"'%s%n%b'"' HEAD^2 | grep "git-subtree.*:")"'' ""
'

# prepare second pair of repositories
mkdir test2
cd test2

test_expect_success 'init main' '
        test_create_repo main
'

cd main

test_expect_success 'add main1' '
        create main1 &&
        git commit -m "main1"
'

cd ..

test_expect_success 'init sub' '
        test_create_repo sub
'

cd sub

test_expect_success 'add sub2' '
        create sub2 &&
        git commit -m "sub2"
'

cd ../main

# check if split can find proper base without --onto

test_expect_success 'add sub as subdir in main' '
        git fetch ../sub master &&
        git branch sub2 FETCH_HEAD &&
        git subtree add --prefix subdir sub2
'

cd ../sub

test_expect_success 'add sub3' '
        create sub3 &&
        git commit -m "sub3"
'

cd ../main

test_expect_success 'merge from sub' '
        git fetch ../sub master &&
        git branch sub3 FETCH_HEAD &&
        git subtree merge --prefix subdir sub3
'

test_expect_success 'add main-sub4' '
        create subdir/main-sub4 &&
        git commit -m "main-sub4"
'

test_expect_success 'split for main-sub4 without --onto' '
        git subtree split --prefix subdir --branch mainsub4
'

# at this point, the new commit parent should be sub3 if it is not,
# something went wrong (the "newparent" of "master~" commit should
# have been sub3, but it was not, because its cache was not set to
# itself)

test_expect_success 'check that the commit parent is sub3' '
        check_equal ''"$(git log --pretty=format:%P -1 mainsub4)"'' ''"$(git rev-parse sub3)"''
'

test_expect_success 'add main-sub5' '
        mkdir subdir2 &&
        create subdir2/main-sub5 &&
        git commit -m "main-sub5"
'

test_expect_success 'split for main-sub5 without --onto' '
        # also test that we still can split out an entirely new subtree
        # if the parent of the first commit in the tree is not empty,
	# then the new subtree has accidentally been attached to something
        git subtree split --prefix subdir2 --branch mainsub5 &&
        check_equal ''"$(git log --pretty=format:%P -1 mainsub5)"'' ""
'

# make sure no patch changes more than one file.  The original set of commits
# changed only one file each.  A multi-file change would imply that we pruned
# commits too aggressively.
joincommits()
{
	commit=
	all=
	while read x y; do
		#echo "{$x}" >&2
		if [ -z "$x" ]; then
			continue
		elif [ "$x" = "commit:" ]; then
			if [ -n "$commit" ]; then
				echo "$commit $all"
				all=
			fi
			commit="$y"
		else
			all="$all $y"
		fi
	done
	echo "$commit $all"
}

test_expect_success 'verify one file change per commit' '
        x= &&
        list=''"$(git log --pretty=format:'"'commit: %H'"' | joincommits)"'' &&
#        test_debug "echo HERE" &&
#        test_debug "echo ''"$list"''" &&
        (git log --pretty=format:'"'commit: %H'"' | joincommits |
        (       while read commit a b; do
		        test_debug "echo Verifying commit "''"$commit"''
		        test_debug "echo a: "''"$a"''
		        test_debug "echo b: "''"$b"''
		        check_equal "$b" ""
		        x=1
	        done
	        check_equal "$x" 1
        ))
'
# Tests for subtree add which creates .gittrees, pull, push, 
# pull-all, push-all and list sub commands

# Back to mainline and create new directory for testing
cd ../..

mkdir test_sub
cd test_sub

mkdir shared_projects
# To shared_projects!
cd shared_projects

# Create couple of Git repos in shared_projects folder which can be
# added as subtrees to our parent projects
test_expect_success 'add subtree1' '
        test_create_repo subtree1 &&
        cd subtree1 &&
        create sub1_file1 &&
        git commit -m "Initial subtree1 commit"
'

# Store the latest commit value for future use
expected_subtreeCommit=`echo $(last_commit_id)`
expected_branch=`echo $(git rev-parse --abbrev-ref HEAD)`

# Back to shared_projects
cd ..

test_expect_success 'add subtree2' '
        test_create_repo subtree2 &&
        cd subtree2 &&
        create sub2_file1 &&
        git commit -m "Initial subtree2 commit"
'

# Back to test_sub
cd ../..

# Create test parent repos that will add subtrees to itself
test_expect_success 'add parent1' '
        test_create_repo parent1 &&
        cd parent1 &&
        create parent1_file1 &&
        git commit -m "Initial parent1 commit"
'

# Back to test_sub from parent1
cd ..

test_expect_success 'add parent2' '
        test_create_repo parent2 &&
        cd parent2 &&
        create parent2_file1 &&
        git commit -m "Initial parent2 commit"
'


# To parent1 now. Start the tests
cd ../parent1

# .gittrees file creation tests
test_expect_success 'check add for subtree with master branch' '
        git subtree add -m "Add sub1 subtree" -P sub1 ../shared_projects/subtree1 master &&
        check_equal ''"$(last_commit_message)"'' "Add sub1 subtree"
'

# Store latest commit id for future use
expected_subtreeMergeCommit=$(last_commit_id)

test_expect_success 'check if .gittrees file was created' '
        test -a '.gittrees'
'
# Now lets test if the .gittrees file has the correct information
# Hardcoded some expected results for checking data inside .gittrees file
expected_url='../shared_projects/subtree1'
expected_path='sub1'

echo $expected_url>>expected_gittrees
echo $expected_path>>expected_gittrees
echo $expected_branch>>expected_gittrees
echo $expected_subtreeCommit>>expected_gittrees
echo $expected_subtreeMergeCommit>>expected_gittrees

grep = .gittrees | cut -f2 -d"=" | cut -f2 -d" " > actual_gittrees

test_expect_success 'check .gittrees file has the necessary changes' '
        test_cmp actual_gittrees expected_gittrees
'

test_expect_success 'check subtree does not get created with incorrect remote url' '
        test_must_fail git subtree add -P s2 ../shared_projects/subbtree1 master
'

test_expect_success 'check that subtree does not get created with incorrect branch' '
        test_must_fail git subtree add -P s2 ../shared_projects/subtree1 development
'

test_expect_success 'add another subtree with master branch' '
        git subtree add -m "Add sub2 subtree" -P sub2 ../shared_projects/subtree2 master &&
        check_equal ''"$(last_commit_message)"'' "Add sub2 subtree"
'

# Lets commit the changes we made to .gittrees file
test_expect_success 'Commit chages to .gittrees for sub1 and sub2 in repo' '
        git add .gittrees &&
        git commit -m "Add .gittrees file"
'
# Tests for subtree list
# Hardcode expected output to a file
cat >expect <<-\EOF
    sub1        (merged from ../shared_projects/subtree1 branch master) 
    sub2        (merged from ../shared_projects/subtree2 branch master) 
EOF

test_expect_success 'check subtree list gives correct output' '
        git subtree list>output &&
        test_cmp expect output
'
# Lets commit the changes to parent1 before proceeding
test_expect_success 'Commit changes to the repository' '
        git add --all &&
        git commit -m "Commit new file additions"
'

# Tests for individual subtree pull using information in .gittrees
# Go to subtree1 and make a change
cd ../shared_projects/subtree1

subtree1_change1="Add_line_to_Sub1_File2"

echo $subtree1_change1>>sub1_file2

# Lets commit the changes to subtree1 before proceeding
test_expect_success 'Commit changes to the subtree1' '
        git add --all &&
        git commit -m "Commit change to sub1_file2"
'

# Switch to develop branch for a future test to push changes to master
test_expect_success 'Switch to branch develop' '
        git checkout -b develop
'

# Back to parent1
cd ../../parent1

test_expect_success 'check  git subtree pull <prefix> works' '
        git subtree pull -P sub1 master &&
        test_cmp sub1/sub1_file1 ../shared_projects/subtree1/sub1_file1 &&
        test_cmp sub1/sub1_file2 ../shared_projects/subtree1/sub1_file2
'

# Now lets make local change on subtree and push it to subtree remote
cd sub1

local_change="Local addition of line to sub1 file 2"
echo $local_change1>>sub1_file2

# Back to parent1
cd ..

# Lets commit the changes to parent1 before proceeding
test_expect_success 'Commit changes to parent repository' '
        git add --all &&
        git commit -m "Commit local changes to sub1/sub1 file2"
'

test_expect_success 'check git subtree push <prefix> works' '
        git subtree push -P sub1 &&
        cd ../shared_projects/subtree1 &&
        git checkout master &&
        test_cmp ../../parent1/sub1/sub1_file1 sub1_file1 &&
        test_cmp ../../parent1/sub1/sub1_file2 sub1_file2
'

# Tests for pull-all

# Make a change in subtree1

subtree1_change2="Add_line_to_Sub1_File1"

echo $subtree1_change2>>sub1_file1

# Lets commit the changes to subtree1 before proceeding
test_expect_success 'Commit changes to the subtree1' '
        git add --all &&
        git commit -m "Commit change to sub1_file1"
'

# Go to subtree2 and make a change
cd ../subtree2

subtree2_change1="Add_line_to_Sub2_File2"

echo $subtree2_change1>>sub2_file1

# Lets commit the changes to subtree2 before proceeding
test_expect_success 'Commit changes to the subtree2' '
        git add --all &&
        git commit -m "Commit change to sub2_file1"
'

# Now subtree pull-all from our parent should bring these changes
cd ../../parent1

test_expect_success 'check pull-all gets all changes from remote subtrees' '
        git subtree pull-all &&
        test_cmp sub1/sub1_file1 ../shared_projects/subtree1/sub1_file1 &&
        test_cmp sub1/sub1_file2 ../shared_projects/subtree1/sub1_file2 &&
        test_cmp sub2/sub2_file1 ../shared_projects/subtree2/sub2_file1
'

# Test for push-all
# Inorder to push to remote, we need the remote's working branch to be different
# to the one we are pushing to. Hence change to develop so that we can push to master
cd ../shared_projects/subtree1
test_expect_success 'Switch to branch develop' '
        git checkout develop
'

cd ../subtree2
test_expect_success 'Switch to branch develop' '
        git checkout -b develop
'

cd ../../parent1
# Now lets make local changes to subtree files and try pushing them
local_change1="Local addition of line to sub1 file 1"
local_change2="Sub2 file 1 has local addition to itself"

cd sub1
echo $local_change1>>sub1_file1

cd ../sub2
echo $local_change2>>sub2_file1

# Back to parent1
cd ..

# Lets commit the changes to our parent repository before proceeding
test_expect_success 'Commit local changes to subtrees of repo' '
        git add --all &&
        git commit -m "Commit local changes made to subtrees"
'

# Now lets do subtree push-all and check on the subtree remote 
# if the changes pushed successfully
test_expect_success 'check if push-all pushes local changes to remote' '
        git subtree push-all &&
        cd ../shared_projects/subtree1 &&
        git checkout master &&
        cd ../subtree2 &&
        git checkout master &&
        cd ../.. &&
        test_cmp parent1/sub1/sub1_file1 shared_projects/subtree1/sub1_file1 &&
        test_cmp parent1/sub2/sub2_file1 shared_projects/subtree2/sub2_file1
'

test_done
