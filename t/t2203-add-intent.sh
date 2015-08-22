#!/bin/sh

test_description='Intent to add'

. ./test-lib.sh

test_expect_success 'intent to add' '
	echo hello >file &&
	echo hello >elif &&
	git add -N file &&
	git add elif
'

test_expect_success 'check result of "add -N"' '
	git ls-files -s file >actual &&
	empty=$(git hash-object --stdin </dev/null) &&
	echo "100644 $empty 0	file" >expect &&
	test_cmp expect actual
'

test_expect_success 'intent to add is just an ordinary empty blob' '
	git add -u &&
	git ls-files -s file >actual &&
	git ls-files -s elif | sed -e "s/elif/file/" >expect &&
	test_cmp expect actual
'

test_expect_success 'intent to add does not clobber existing paths' '
	git add -N file elif &&
	empty=$(git hash-object --stdin </dev/null) &&
	git ls-files -s >actual &&
	! grep "$empty" actual
'

test_expect_success 'i-t-a entry is simply ignored' '
	test_tick &&
	git commit -a -m initial &&
	git reset --hard &&

	echo xyzzy >rezrov &&
	echo frotz >nitfol &&
	git add rezrov &&
	git add -N nitfol &&
	git commit -m second &&
	test $(git ls-tree HEAD -- nitfol | wc -l) = 0 &&
	test $(git diff --name-only HEAD -- nitfol | wc -l) = 1
'

test_expect_success 'can commit with an unrelated i-t-a entry in index' '
	git reset --hard &&
	echo bozbar >rezrov &&
	echo frotz >nitfol &&
	git add rezrov &&
	git add -N nitfol &&
	git commit -m partial rezrov
'

test_expect_success 'can "commit -a" with an i-t-a entry' '
	git reset --hard &&
	: >nitfol &&
	git add -N nitfol &&
	git commit -a -m all
'

test_expect_success 'cache-tree invalidates i-t-a paths' '
	git reset --hard &&
	mkdir dir &&
	: >dir/foo &&
	git add dir/foo &&
	git commit -m foo &&

	: >dir/bar &&
	git add -N dir/bar &&
	git diff --cached --name-only >actual &&
	echo dir/bar >expect &&
	test_cmp expect actual &&

	git write-tree >/dev/null &&

	git diff --cached --name-only >actual &&
	echo dir/bar >expect &&
	test_cmp expect actual
'

test_expect_success 'apply adds new file on i-t-a entry' '
	git init apply &&
	(
		cd apply &&
		echo newcontent >newfile &&
		git add newfile &&
		git diff --cached >patch &&
		rm .git/index &&
		git add -N newfile &&
		git apply --cached patch
	)
'

test_expect_success 'apply:check_preimage() not creating empty file' '
	git init check-preimage &&
	(
		cd check-preimage &&
		echo oldcontent >newfile &&
		git add newfile &&
		echo newcontent >newfile &&
		git diff >patch &&
		rm .git/index &&
		git add -N newfile &&
		rm newfile &&
		test_must_fail git apply -3 patch &&
		! test -f newfile
	)
'

test_expect_success 'checkout ignores i-t-a' '
	git init checkout &&
	(
		cd checkout &&
		echo data >file &&
		git add -N file &&
		test_must_fail git checkout -- file &&
		echo data >expected &&
		test_cmp expected file
	)
'

test_expect_success 'checkout-index ignores i-t-a' '
	(
		cd checkout &&
		git checkout-index file &&
		echo data >expected &&
		test_cmp expected file
	)
'

test_expect_success 'checkout-index --all ignores i-t-a' '
	(
		cd checkout &&
		echo data >anotherfile &&
		git add anotherfile &&
		rm anotherfile &&
		git checkout-index --all &&
		echo data >expected &&
		test_cmp expected file &&
		test_cmp expected anotherfile
	)
'

test_done

