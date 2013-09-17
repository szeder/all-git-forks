#!/bin/sh

test_description='git add v2.0 behavior'

. ./test-lib.sh

test_expect_success setup '
	mkdir dir1 &&
	echo one > dir1/content &&
	echo one > dir1/to-remove &&
	git add . &&
	git commit -m one
'

test_expect_success 'update in dir throws warning' '
	test_when_finished "git reset --hard" &&
	echo two > dir1/content &&
	mkdir -p dir2 &&
	(
	cd dir2 &&
	git add -u 2> err &&
	cat err &&
	grep "will change in Git 2.0" err
	)
'

test_expect_success 'update in dir updates everything' '
	test_when_finished "git reset --hard" &&
	test_config core.mode next &&
	echo two > dir1/content &&
	mkdir -p dir2 &&
	(
	cd dir2 &&
	git add -u 2> err &&
	cat err &&
	! grep "will change in Git 2.0" err
	) &&
	test "$(git ls-files -m)" = ""
'

test_expect_success 'default to ignore removal' '
	test_when_finished "git reset --hard" &&
	rm dir1/to-remove &&
	git add dir1 2> err &&
	cat err &&
	grep "will change in Git 2.0" err &&
	test "$(git ls-files -c)" != "dir1/content"
'

test_expect_success 'default adds removals' '
	test_when_finished "git reset --hard" &&
	test_config core.mode next &&
	rm dir1/to-remove &&
	git add dir1 2> err &&
	cat err &&
	! grep "will change in Git 2.0" err &&
	test "$(git ls-files -c)" = "dir1/content"
'

test_done
