#!/bin/sh

test_description='hunk edit with "commit -p -m"'
. ./test-lib.sh

if ! test_have_prereq PERL
then
	skip_all="skipping '$test_description' tests, perl not available"
	test_done
fi

test_expect_success 'setup (initial)' '
	echo line1 >file &&
	git add file &&
	git commit -m commit1 &&
	echo line3 >>file &&
	cat >expect <<-\EOF
	diff --git a/file b/file
	index a29bdeb..c0d0fb4 100644
	--- a/file
	+++ b/file
	@@ -1 +1,2 @@
	 line1
	+line2
	EOF
'

test_expect_success 'edit hunk "commit -p -m message"' '
	echo e | env GIT_EDITOR="sed s/+line3\$/+line2/ -i" git commit -p -m commit2 file &&
	git diff HEAD^ HEAD >actual &&
	test_cmp expect actual
'

test_done
