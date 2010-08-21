#!/bin/sh

test_description='read-tree in narrow mode'

. ./test-lib.sh

test_expect_success setup '
	test_tick &&
	test_commit 1 &&
	mkdir t1 t2 t1/t12 &&
	echo 0 >f0 &&
	echo 10 >t1/f10 &&
	echo 120 >t1/t12/f120 &&
	echo 20 >t2/f20
	git add t1 t2 f0 && git commit -m initial &&
	HEAD=`git rev-parse HEAD` &&
	git rev-parse HEAD | git pack-objects --revs pack -- t1/t12 &&
	test_create_repo narrow &&
	mv pack-* narrow/.git/objects/pack &&
	cd narrow &&
	echo $HEAD >.git/refs/heads/master &&
	echo "ref: refs/heads/master" >.git/HEAD &&
	echo t1/t12 >.git/narrow
'

test_expect_failure ls-tree '
	git ls-tree -r HEAD &&
	git ls-files --stage >result &&
	echo "100644 blob 52bd8e43afb01d0c9747f1fedf2fc94684ee4cc4	t1/t12/f120" >expected &&
	test_cmp expected result
'

test_expect_success read-tree '
	git read-tree HEAD &&
	git ls-files --stage >result &&
	echo "100644 52bd8e43afb01d0c9747f1fedf2fc94684ee4cc4 0	t1/t12/f120" >expected &&
	test_cmp expected result
'

test_expect_success checkout '
	git checkout . &&
	test_cmp ../t1/t12/f120 t1/t12/f120
'

cat <<EOF >diff.expected
diff --git a/t1/t12/f120 b/t1/t12/f120
index 52bd8e4..645fb94 100644
--- a/t1/t12/f120
+++ b/t1/t12/f120
@@ -1 +1,2 @@
 120
+modified
EOF

test_expect_success diff '
	echo modified >>t1/t12/f120 &&
	git diff >result &&
	test_cmp diff.expected result
'

test_expect_success 'diff HEAD' '
	git diff HEAD >result &&
	test_cmp diff.expected result
'

test_expect_success 'diff --cached' '
	git add -u . &&
	git diff --cached >result &&
	test_cmp diff.expected result
'

test_done
