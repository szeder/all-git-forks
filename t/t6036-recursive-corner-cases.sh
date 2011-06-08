#!/bin/sh

test_description='recursive merge corner cases involving criss-cross merges'

. ./test-lib.sh

#
#  L1  L2
#   o---o
#  / \ / \
# o   X   ?
#  \ / \ /
#   o---o
#  R1  R2
#

test_expect_success 'setup basic criss-cross + rename with no modifications' '
	ten="0 1 2 3 4 5 6 7 8 9" &&
	for i in $ten
	do
		echo line $i in a sample file
	done >one &&
	for i in $ten
	do
		echo line $i in another sample file
	done >two &&
	git add one two &&
	test_tick && git commit -m initial &&

	git branch L1 &&
	git checkout -b R1 &&
	git mv one three &&
	test_tick && git commit -m R1 &&

	git checkout L1 &&
	git mv two three &&
	test_tick && git commit -m L1 &&

	git checkout L1^0 &&
	test_tick && git merge -s ours R1 &&
	git tag L2 &&

	git checkout R1^0 &&
	test_tick && git merge -s ours L1 &&
	git tag R2
'

test_expect_success 'merge simple rename+criss-cross with no modifications' '
	git reset --hard &&
	git checkout L2^0 &&

	test_must_fail git merge -s recursive R2^0 &&

	test 5 = $(git ls-files -s | wc -l) &&
	test 3 = $(git ls-files -u | wc -l) &&
	test 0 = $(git ls-files -o | wc -l) &&

	test $(git rev-parse :0:one) = $(git rev-parse L2:one) &&
	test $(git rev-parse :0:two) = $(git rev-parse R2:two) &&
	test $(git rev-parse :2:three) = $(git rev-parse L2:three) &&
	test $(git rev-parse :3:three) = $(git rev-parse R2:three) &&

	cp one merged &&
	>empty &&
	test_must_fail git merge-file \
		-L "Temporary merge branch 1" \
		-L "" \
		-L "Temporary merge branch 2" \
		merged empty two &&
	test $(git rev-parse :1:three) = $(git hash-object merged)
'

#
# Same as before, but modify L1 slightly:
#
#  L1m L2
#   o---o
#  / \ / \
# o   X   ?
#  \ / \ /
#   o---o
#  R1  R2
#

test_expect_success 'setup criss-cross + rename merges with basic modification' '
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	ten="0 1 2 3 4 5 6 7 8 9"
	for i in $ten
	do
		echo line $i in a sample file
	done >one &&
	for i in $ten
	do
		echo line $i in another sample file
	done >two &&
	git add one two &&
	test_tick && git commit -m initial &&

	git branch L1 &&
	git checkout -b R1 &&
	git mv one three &&
	echo more >>two &&
	git add two &&
	test_tick && git commit -m R1 &&

	git checkout L1 &&
	git mv two three &&
	test_tick && git commit -m L1 &&

	git checkout L1^0 &&
	test_tick && git merge -s ours R1 &&
	git tag L2 &&

	git checkout R1^0 &&
	test_tick && git merge -s ours L1 &&
	git tag R2
'

test_expect_success 'merge criss-cross + rename merges with basic modification' '
	git reset --hard &&
	git checkout L2^0 &&

	test_must_fail git merge -s recursive R2^0 &&

	test 5 = $(git ls-files -s | wc -l) &&
	test 3 = $(git ls-files -u | wc -l) &&
	test 0 = $(git ls-files -o | wc -l) &&

	test $(git rev-parse :0:one) = $(git rev-parse L2:one) &&
	test $(git rev-parse :0:two) = $(git rev-parse R2:two) &&
	test $(git rev-parse :2:three) = $(git rev-parse L2:three) &&
	test $(git rev-parse :3:three) = $(git rev-parse R2:three) &&

	head -n 10 two >merged &&
	cp one merge-me &&
	>empty &&
	test_must_fail git merge-file \
		-L "Temporary merge branch 1" \
		-L "" \
		-L "Temporary merge branch 2" \
		merge-me empty merged &&

	test $(git rev-parse :1:three) = $(git hash-object merge-me)
'

#
# For the next test, we start with three commits in two lines of development
# which setup a rename/add conflict:
#   Commit A: File 'a' exists
#   Commit B: Rename 'a' -> 'new_a'
#   Commit C: Modify 'a', create different 'new_a'
# Later, two different people merge and resolve differently:
#   Commit D: Merge B & C, ignoring separately created 'new_a'
#   Commit E: Merge B & C making use of some piece of secondary 'new_a'
# Finally, someone goes to merge D & E.  Does git detect the conflict?
#
#      B   D
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E
#

test_expect_success 'setup differently handled merges of rename/add conflict' '
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	printf "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n" >a &&
	git add a &&
	test_tick && git commit -m A &&

	git branch B &&
	git checkout -b C &&
	echo 10 >>a &&
	echo "other content" >>new_a &&
	git add a new_a &&
	test_tick && git commit -m C &&

	git checkout B &&
	git mv a new_a &&
	test_tick && git commit -m B &&

	git checkout B^0 &&
	test_must_fail git merge C &&
	git clean -f &&
	test_tick && git commit -m D &&
	git tag D &&

	git checkout C^0 &&
	test_must_fail git merge B &&
	rm new_a~HEAD new_a &&
	printf "Incorrectly merged content" >>new_a &&
	git add -u &&
	test_tick && git commit -m E &&
	git tag E
'

test_expect_success 'git detects differently handled merges conflict' '
	git reset --hard &&
	git checkout D^0 &&

	git merge -s recursive E^0 && {
		echo "BAD: should have conflicted"
		test "Incorrectly merged content" = "$(cat new_a)" &&
			echo "BAD: Silently accepted wrong content"
		return 1
	}

	test 3 = $(git ls-files -s | wc -l) &&
	test 3 = $(git ls-files -u | wc -l) &&
	test 0 = $(git ls-files -o | wc -l) &&

	test $(git rev-parse :2:new_a) = $(git rev-parse D:new_a) &&
	test $(git rev-parse :3:new_a) = $(git rev-parse E:new_a) &&

	git cat-file -p B:new_a >>merged &&
	git cat-file -p C:new_a >>merge-me &&
	>empty &&
	test_must_fail git merge-file \
		-L "Temporary merge branch 2" \
		-L "" \
		-L "Temporary merge branch 1" \
		merged empty merge-me &&
	test $(git rev-parse :1:new_a) = $(git hash-object merged)
'

#
# criss-cross + modify/delete:
#
#      B   D
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E
#
#   Commit A: file with contents 'A\n'
#   Commit B: file with contents 'B\n'
#   Commit C: file not present
#   Commit D: file with contents 'B\n'
#   Commit E: file not present
#
# Now, when we merge commits D & E, does git detect the conflict?

test_expect_success 'setup criss-cross + modify/delete resolved differently' '
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	echo A >file &&
	git add file &&
	test_tick &&
	git commit -m A &&

	git branch B &&
	git checkout -b C &&
	git rm file &&
	test_tick &&
	git commit -m C &&

	git checkout B &&
	echo B >file &&
	git add file &&
	test_tick &&
	git commit -m B &&

	git checkout B^0 &&
	test_must_fail git merge C &&
	echo B >file &&
	git add file &&
	test_tick &&
	git commit -m D &&
	git tag D &&

	git checkout C^0 &&
	test_must_fail git merge B &&
	git rm file &&
	test_tick &&
	git commit -m E &&
	git tag E
'

test_expect_success 'git detects conflict merging criss-cross+modify/delete' '
	git checkout D^0 &&

	test_must_fail git merge -s recursive E^0 &&

	test 2 -eq $(git ls-files -s | wc -l) &&
	test 2 -eq $(git ls-files -u | wc -l) &&

	test $(git rev-parse :1:file) = $(git rev-parse master:file) &&
	test $(git rev-parse :2:file) = $(git rev-parse B:file)
'

test_expect_success 'git detects conflict merging criss-cross+modify/delete, reverse direction' '
	git reset --hard &&
	git checkout E^0 &&

	test_must_fail git merge -s recursive D^0 &&

	test 2 -eq $(git ls-files -s | wc -l) &&
	test 2 -eq $(git ls-files -u | wc -l) &&

	test $(git rev-parse :1:file) = $(git rev-parse master:file) &&
	test $(git rev-parse :3:file) = $(git rev-parse B:file)
'

#
# criss-cross + modify/modify with very contrived file contents:
#
#      B   D
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E
#
#   Commit A: file with contents 'A\n'
#   Commit B: file with contents 'B\n'
#   Commit C: file with contents 'C\n'
#   Commit D: file with contents 'D\n'
#   Commit E: file with contents:
#      <<<<<<< Temporary merge branch 1
#      C
#      =======
#      B
#      >>>>>>> Temporary merge branch 2
#
# Now, when we merge commits D & E, does git detect the conflict?

test_expect_success 'setup differently handled merges of content conflict' '
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	echo A >file &&
	git add file &&
	test_tick &&
	git commit -m A &&

	git branch B &&
	git checkout -b C &&
	echo C >file &&
	git add file &&
	test_tick &&
	git commit -m C &&

	git checkout B &&
	echo B >file &&
	git add file &&
	test_tick &&
	git commit -m B &&

	git checkout B^0 &&
	test_must_fail git merge C &&
	echo D >file &&
	git add file &&
	test_tick &&
	git commit -m D &&
	git tag D &&

	git checkout C^0 &&
	test_must_fail git merge B &&
	cat <<EOF >file &&
<<<<<<< Temporary merge branch 1
C
=======
B
>>>>>>> Temporary merge branch 2
EOF
	git add file &&
	test_tick &&
	git commit -m E &&
	git tag E
'

test_expect_failure 'git detects conflict w/ criss-cross+contrived resolution' '
	git checkout D^0 &&

	test_must_fail git merge -s recursive E^0 &&

	test 3 -eq $(git ls-files -s | wc -l) &&
	test 3 -eq $(git ls-files -u | wc -l) &&
	test 0 -eq $(git ls-files -o | wc -l) &&

	test $(git rev-parse :2:file) = $(git rev-parse D:file) &&
	test $(git rev-parse :3:file) = $(git rev-parse E:file)
'

#
# criss-cross + d/f conflict via add/add:
#   Commit A: Neither file 'a' nor directory 'a/' exist.
#   Commit B: Introduce 'a'
#   Commit C: Introduce 'a/file'
# Two different later cases:
#   Commit D1: Merge B & C, keeping 'a' and deleting 'a/'
#   Commit E1: Merge B & C, deleting 'a' but keeping 'a/file'
#
#   Commit D2: Merge B & C, keeping a modified 'a' and deleting 'a/'
#   Commit E2: Merge B & C, deleting 'a' but keeping a modified 'a/file'
#
#   Note: D == D1.
# Finally, someone goes to merge D1&E1 or D1&E2 or D2&E1.  What happens?
#
#      B   D1 or D2
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E1 or E2
#

test_expect_success 'setup differently handled merges of directory/file conflict' '
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	>irrelevant-file &&
	git add irrelevant-file &&
	test_tick &&
	git commit -m A &&

	git branch B &&
	git checkout -b C &&
	mkdir a &&
	echo 10 >a/file &&
	git add a/file &&
	test_tick &&
	git commit -m C &&

	git checkout B &&
	echo 5 >a &&
	git add a &&
	test_tick &&
	git commit -m B &&

	git checkout B^0 &&
	test_must_fail git merge C &&
	git clean -f &&
	rm -rf a/ &&
	echo 5 >a &&
	git add a &&
	test_tick &&
	git commit -m D &&
	git tag D &&

	git checkout C^0 &&
	test_must_fail git merge B &&
	git clean -f &&
	git rm --cached a &&
	echo 10 >a/file &&
	git add a/file &&
	test_tick &&
	git commit -m E1 &&
	git tag E1 &&

	git checkout C^0 &&
	test_must_fail git merge B &&
	git clean -f &&
	git rm --cached a &&
	printf "10\n11\n" >a/file &&
	git add a/file &&
	test_tick &&
	git commit -m E2 &&
	git tag E2
'

test_expect_success 'git detects conflict and handles merge of D & E1 correctly' '
	git reset --hard &&
	git reset --hard &&
	git clean -fdqx &&
	git checkout D^0 &&

	# FIXME: If merge-base could keep both a and a/file in its tree, then
	# we could this merge would actually be able to succeed.
	test_must_fail git merge -s recursive E1^0 &&

	test 2 -eq $(git ls-files -s | wc -l) &&
	test 1 -eq $(git ls-files -u | wc -l) &&
	test 0 -eq $(git ls-files -o | wc -l) &&

	test $(git rev-parse :2:a) = $(git rev-parse B:a)
'

test_expect_success 'git detects conflict and handles merge of E1 & D correctly' '
	git reset --hard &&
	git reset --hard &&
	git clean -fdqx &&
	git checkout E1^0 &&

	# FIXME: If merge-base could keep both a and a/file in its tree, then
	# we could this merge would actually be able to succeed.
	test_must_fail git merge -s recursive D^0 &&

	test 2 -eq $(git ls-files -s | wc -l) &&
	test 1 -eq $(git ls-files -u | wc -l) &&
	test 0 -eq $(git ls-files -o | wc -l) &&

	test $(git rev-parse :3:a) = $(git rev-parse B:a)
'

test_expect_success 'git detects conflict and handles merge of D & E2 correctly' '
	git reset --hard &&
	git reset --hard &&
	git clean -fdqx &&
	git checkout D^0 &&

	test_must_fail git merge -s recursive E2^0 &&

	test 3 -eq $(git ls-files -s | wc -l) &&
	test 2 -eq $(git ls-files -u | wc -l) &&
	test 1 -eq $(git ls-files -o | wc -l) &&

	test $(git rev-parse :2:a) = $(git rev-parse B:a) &&
	test $(git rev-parse :3:a/file) = $(git rev-parse E1:a/file)
	test $(git rev-parse :1:a/file) = $(git rev-parse C:a/file)
'

test_expect_success 'git detects conflict and handles merge of E2 & D correctly' '
	git reset --hard &&
	git reset --hard &&
	git clean -fdqx &&
	git checkout E2^0 &&

	test_must_fail git merge -s recursive D^0 &&

	test 3 -eq $(git ls-files -s | wc -l) &&
	test 2 -eq $(git ls-files -u | wc -l) &&
	test 1 -eq $(git ls-files -o | wc -l) &&

	test $(git rev-parse :3:a) = $(git rev-parse B:a) &&
	test $(git rev-parse :2:a/file) = $(git rev-parse E1:a/file)
	test $(git rev-parse :1:a/file) = $(git rev-parse C:a/file)
'

#
# criss-cross with rename/rename(1to2)/modify followed by
# rename/rename(2to1)/modify:
#
#      B   D
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E
#
#   Commit A: new file: a
#   Commit B: rename a->b, modifying by adding a line
#   Commit C: rename a->c
#   Commit D: merge B&C, resolving conflict by keeping contents in newname
#   Commit E: merge B&C, resolving conflict similar to D but adding another line
#
# There is a conflict merging B & C, but one of filename not of file
# content.  Whoever created D and E chose specific resolutions for that
# conflict resolution.  Now, since: (1) there is no content conflict
# merging B & C, (2) D does not modify that merged content further, and (3)
# both D & E resolve the name conflict in the same way, the modification to
# newname in E should not cause any conflicts when it is merged with D.
# (Note that this can be accomplished by having the virtual merge base have
# the merged contents of b and c stored in a file named a, which seems like
# the most logical choice anyway.)

test_expect_success 'setup rename/rename(1to2)/modify followed by what looks like rename/rename(2to1)/modify' '
	git reset --hard &&
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	printf "1\n2\n3\n4\n5\n6\n" >a &&
	git add a &&
	git commit -m A &&
	git tag A &&

	git checkout -b B A &&
	git mv a b &&
	echo 7 >>b &&
	git add -u &&
	git commit -m B &&

	git checkout -b C A &&
	git mv a c &&
	git commit -m C &&

	git checkout -q B^0 &&
	git merge --no-commit -s ours C^0 &&
	git mv b newname &&
	git commit -m "Merge commit C^0 into HEAD" &&
	git tag D &&

	git checkout -q C^0 &&
	git merge --no-commit -s ours B^0 &&
	git mv c newname &&
	printf "7\n8\n" >>newname &&
	git add -u &&
	git commit -m "Merge commit B^0 into HEAD" &&
	git tag E
'

test_expect_success 'handle rename/rename(1to2)/modify followed by what looks like rename/rename(2to1)/modify' '
	git checkout D^0 &&

	git merge -s recursive E^0 &&

	test 1 -eq $(git ls-files -s | wc -l) &&
	test 0 -eq $(git ls-files -u | wc -l) &&
	test 0 -eq $(git ls-files -o | wc -l) &&

	test 8 -eq $(wc -l < newname)
'

#
# criss-cross with rename/rename(1to2)/add-source + resolvable modify/modify:
#
#      B   D
#      o---o
#     / \ / \
#  A o   X   ? F
#     \ / \ /
#      o---o
#      C   E
#
#   Commit A: new file: a
#   Commit B: rename a->b
#   Commit C: rename a->c, add different a
#   Commit D: merge B&C, keeping b&c and (new) a modified at beginning
#   Commit E: merge B&C, keeping b&c and (new) a modified at end
#
# Now, when we merge commits D & E, there should be no conflict...

test_expect_success 'setup criss-cross + rename/rename/add + modify/modify' '
	git rm -rf . &&
	git clean -fdqx &&
	rm -rf .git &&
	git init &&

	printf "lots\nof\nwords\nand\ncontent\n" >a &&
	git add a &&
	git commit -m A &&
	git tag A &&

	git checkout -b B A &&
	git mv a b &&
	git commit -m B &&

	git checkout -b C A &&
	git mv a c &&
	printf "2\n3\n4\n5\n6\n7\n" >a &&
	git add a &&
	git commit -m C &&

	git checkout B^0 &&
	git merge --no-commit -s ours C^0 &&
	git checkout C -- a c &&
	mv a old_a &&
	echo 1 >a &&
	cat old_a >>a &&
	rm old_a &&
	git add -u &&
	git commit -m "Merge commit C^0 into HEAD" &&
	git tag D &&

	git checkout C^0 &&
	git merge --no-commit -s ours B^0 &&
	git checkout B -- b &&
	echo 8 >> a &&
	git add -u &&
	git commit -m "Merge commit B^0 into HEAD" &&
	git tag E
'

test_expect_failure 'correctly resolves criss-cross with rename/rename/add and modify/modify conflict' '
	git checkout D^0 &&

	git merge -s recursive E^0 &&

	test 3 -eq $(git ls-files -s | wc -l) &&
	test 0 -eq $(git ls-files -u | wc -l) &&
	test 0 -eq $(git ls-files -o | wc -l) &&

	test 6 -eq $(wc -l < a)
'

test_done
