#!/bin/sh
#
# Copyright (c) 2016 Aaron M. Watson
#

test_description='Test git stash with index ref specification'

. ./test-lib.sh

test_expect_success 'stash some dirty working directory' '
	echo 1 > file &&
	git add file &&
	echo unrelated >other-file &&
	git add other-file &&
	test_tick &&
	git commit -m initial &&
	echo 2 > file &&
	git add file &&
	echo 3 > file &&
	test_tick &&
	git stash &&
	git diff-files --quiet &&
	git diff-index --cached --quiet HEAD
'

cat > expect << EOF
diff --git a/file b/file
index 0cfbf08..00750ed 100644
--- a/file
+++ b/file
@@ -1 +1 @@
-2
+3
EOF

test_expect_success 'applying bogus stash does nothing' '
	test_must_fail git stash apply 1 &&
	echo 1 >expect &&
	test_cmp expect file
'

test_expect_success 'drop middle stash' '
	git reset --hard &&
	echo 8 > file &&
	git stash &&
	echo 9 > file &&
	git stash &&
	git stash drop 1 &&
	test 2 = $(git stash list | wc -l) &&
	git stash apply &&
	test 9 = $(cat file) &&
	test 1 = $(git show :file) &&
	test 1 = $(git show HEAD:file) &&
	git reset --hard &&
	git stash drop &&
	git stash apply &&
	test 3 = $(cat file) &&
	test 1 = $(git show :file) &&
	test 1 = $(git show HEAD:file)
'

test_expect_success 'invalid ref of the form "n", where n >= N' '
	git stash clear &&
	test_must_fail git stash drop 0 &&
	echo bar5 > file &&
	echo bar6 > file2 &&
	git add file2 &&
	git stash &&
	test_must_fail git stash drop 1 &&
	test_must_fail git stash pop 1 &&
	test_must_fail git stash apply 1 &&
	test_must_fail git stash show 1 &&
	test_must_fail git stash branch tmp 1 &&
	git stash drop
'

test_done
