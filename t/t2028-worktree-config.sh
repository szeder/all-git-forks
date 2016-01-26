#!/bin/sh

test_description="config file in multi worktree"

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit start &&
	git worktree add wt1 &&
	git worktree add wt2
'

test_expect_success 'main config is shared in version 0' '
	git config -f .git/config wt.name main &&
	git config wt.name >actual &&
	echo main >expected &&
	test_cmp expected actual &&
	git -C wt1 config wt.name >actual &&
	test_cmp expected actual
'

test_expect_success 'config --repo on v0' '
	git config --global new.var old-value &&
	git config --repo new.var new-value &&
	test_path_is_missing .git/common/config &&
	git config --repo new.var >actual &&
	echo new-value >expected &&
	test_cmp expected actual
'

test_expect_success 'main config is for main worktree only (v1)' '
	mkdir .git/common &&
	git config -f .git/common/config extensions.worktree 1 &&
	git config wt.name >actual &&
	echo main >expected &&
	test_cmp expected actual &&
	test_must_fail git -C wt1 config wt.name
'

test_expect_success 'common/config is shared (v1)' '
	git config -f .git/common/config some.thing is-shared &&
	echo is-shared >expected &&
	git config some.thing >actual &&
	test_cmp expected actual &&
	git -C wt1 config some.thing >actual &&
	test_cmp expected actual
'

test_expect_success 'config --repo on v1' '
	git config --global new.var1 old-value &&
	git config --repo new.var1 new-value &&
	grep var1 .git/common/config >/dev/null &&
	git config --repo new.var1 >actual &&
	echo new-value >expected &&
	test_cmp expected actual &&
	git -C wt2 config --repo new.var1 >actual &&
	echo new-value >expected &&
	test_cmp expected actual
'

test_done
