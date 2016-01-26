#!/bin/sh

test_description="config file in multi worktree"

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit start &&
	git worktree add --version=0 wt1 &&
	git worktree add --version=0 wt2
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

test_expect_success 'prepare worktree v0' '
	test_create_repo repo-v0 &&
	(
		cd repo-v0 &&
		test_commit v0 &&
		git config core.sparsecheckout true &&
		git config core.ignorestat true &&
		git config core.worktree "$TEST_DIRECTORY" &&
		git config share.key value
	)
'

test_expect_success 'migrate v0 to v1' '
	git -C repo-v0 worktree add --version=1 wt
'

test_expect_success 'after migration: main wortree has extensions.worktree' '
	test "`git -C repo-v0 config core.repositoryformatversion`" = 1 &&
	test "`git -C repo-v0 config extensions.worktree`" = 1
'

test_expect_success 'after migration: linked wortree has extensions.worktree' '
	test "`git -C repo-v0/wt config core.repositoryformatversion`" = 1 &&
	test "`git -C repo-v0/wt config extensions.worktree`" = 1
'

test_expect_success 'after migration: main wortree keeps per-worktree vars' '
	test "`git -C repo-v0 config core.sparsecheckout`" = true &&
	test "`git -C repo-v0 config core.ignorestat`" = true &&
	test "`git -C repo-v0 config core.worktree`" = "$TEST_DIRECTORY"
'

test_expect_success 'after migration: linked wortree has no per-worktree vars' '
	test_must_fail git -C repo-v0/wt config core.sparsecheckout &&
	test_must_fail git -C repo-v0/wt config core.ignorestat &&
	test_must_fail git -C repo-v0/wt config core.worktree
'

test_expect_success 'after migration: shared vars are shared' '
	test "`git -C repo-v0 config share.key`" = value &&
	test "`git -C repo-v0/wt config share.key`" = value
'

test_done
