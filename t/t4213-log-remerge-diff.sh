#!/bin/sh

test_description='test log --remerge-diff'
. ./test-lib.sh

# A ----------
# | \  \      \
# |  C  \      \
# B  |\  \      |
# |  | |  D    unrelated_file
# |\ | |__|__   |
# | X  |_ |  \  |
# |/ \/  \|   \ |
# M1 M2   M3   M4
# ^  ^    ^     ^
# |  |    dm    unrelated
# |  evil
# benign
#
#
# M1 has a "benign" conflict
# M2 has an "evil" conflict: it ignores the changes in D
# M3 has a delete/modify conflict, resolved in favor of a modification
# M4 is a merge of an unrelated change, without conflicts

test_expect_success 'setup' '
	test_commit A file original &&
	test_commit B file change &&
	git checkout -b side A &&
	test_commit C file side &&
	git checkout -b delete A &&
	git rm file &&
	test_commit D &&
	git checkout -b benign master &&
	test_must_fail git merge C &&
	test_commit M1 file merged &&
	git checkout -b evil B &&
	test_must_fail git merge C &&
	test_commit M2 file change &&
	git checkout -b dm C &&
	test_must_fail git merge D &&
	test_commit M3 file resolved &&
	git checkout -b unrelated A &&
	test_commit unrelated_file &&
	git merge C &&
	test_tick &&
	git tag M4 &&
	git branch -D master side
'

test_expect_success 'unrelated merge: without conflicts' '
	git log -p --cc unrelated >expected &&
	git log -p --remerge-diff unrelated >actual &&
	test_cmp expected actual
'

clean_output () {
	git name-rev --name-only --stdin |
	# strip away bits that aren't treated by the above
	sed -e 's/^\(index\|Merge:\|Date:\).*/\1/'
}

cat >expected <<EOF
commit benign
Merge:
Author: A U Thor <author@example.com>
Date:

    M1

diff --git a/file b/file
index
--- a/file
+++ b/file
@@ -1,5 +1 @@
-<<<<<<< tags/B
-change
-=======
-side
->>>>>>> tags/C
+merged
EOF

test_expect_success 'benign merge: conflicts resolved' '
	git log -1 -p --remerge-diff benign >output &&
	clean_output <output >actual &&
	test_cmp expected actual
'

cat >expected <<EOF
commit evil
Merge:
Author: A U Thor <author@example.com>
Date:

    M2

diff --git a/file b/file
index
--- a/file
+++ b/file
@@ -1,5 +1 @@
-<<<<<<< tags/B
 change
-=======
-side
->>>>>>> tags/C
EOF

test_expect_success 'evil merge: changes ignored' '
	git log -1 --remerge-diff -p evil >output &&
	clean_output <output >actual &&
	test_cmp expected actual
'

cat >expected <<EOF
commit dm
Merge:
Author: A U Thor <author@example.com>
Date:

    M3

diff --git a/file b/file
index
--- a/file
+++ b/file
@@ -1,5 +1 @@
-<<<<<<< tags/B
 change
-=======
->>>>>>> tags/D
+resolved
EOF

# The above just one idea what the output might be.  It's not clear
# yet what the best solution is.

test_expect_failure 'delete/modify conflict' '
	git log -1 --remerge-diff -p dm >output &&
	clean_output <output >actual &&
	test_cmp expected actual
'

test_done
