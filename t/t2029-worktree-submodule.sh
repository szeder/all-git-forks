#!/bin/sh

test_description='submodule with multiple worktrees'

. ./test-lib.sh

test_expect_success 'setup' '
	git config extensions.worktreeConfig true &&
	>t &&
	git add t &&
	git commit -m initial &&
	git branch initial
'

test_expect_success 'setup - repository in init subdirectory' '
	mkdir init &&
	(
		cd init &&
		git init &&
		git config extensions.worktreeConfig true &&
		echo a >a &&
		git add a &&
		git commit -m "submodule commit 1" &&
		git tag -a -m "rev-1" rev-1
	)
'

test_expect_success 'setup - commit with gitlink' '
	echo a >a &&
	echo z >z &&
	git add a init z &&
	git commit -m "super commit 1"
'

test_expect_success 'setup - hide init subdirectory' '
	mv init .subrepo
'

test_expect_success 'setup - repository to add submodules to' '
	git init addtest &&
	git -C addtest config extensions.worktreeConfig true &&
	git init addtest-ignore &&
	git -C addtest-ignore config extensions.worktreeConfig true
'

# The 'submodule add' tests need some repository to add as a submodule.
# The trash directory is a good one as any. We need to canonicalize
# the name, though, as some tests compare it to the absolute path git
# generates, which will expand symbolic links.
submodurl=$(pwd -P)

listbranches() {
	git for-each-ref --format='%(refname)' 'refs/heads/*'
}

inspect() {
	dir=$1 &&
	dotdot="${2:-..}" &&

	(
		cd "$dir" &&
		listbranches >"$dotdot/heads" &&
		{ git symbolic-ref HEAD || :; } >"$dotdot/head" &&
		git rev-parse HEAD >"$dotdot/head-sha1" &&
		git update-index --refresh &&
		git diff-files --exit-code &&
		git clean -n -d -x >"$dotdot/untracked"
	)
}

test_expect_success 'submodule add' '
	echo "refs/heads/master" >expect &&
	>empty &&

	(
		cd addtest &&
		git submodule add -q "$submodurl" submod >actual &&
		test_must_be_empty actual &&
		echo "gitdir: ../.git/modules/submod" >expect &&
		test_cmp expect submod/.git &&
		(
			cd submod &&
			git config core.worktree >actual &&
			echo "../../../submod" >expect &&
			test_cmp expect actual &&
			rm -f actual expect
		) &&
		git submodule init
	) &&

	rm -f heads head untracked &&
	inspect addtest/submod ../.. &&
	test_cmp expect heads &&
	test_cmp expect head &&
	test_cmp empty untracked
'

test_expect_success 'submodule.* in supermodule is per-worktree' '
	(
		cd addtest &&
		git config -f .git/config.worktree submodule.submod.url >actual &&
		echo "$submodurl" >expect &&
		test_cmp expect actual
	)
'

test_expect_success 'turn submodule to multiworktree' '
	(
		cd addtest/.git/modules/submod &&
		CORE_WT="$(git config core.worktree)" &&
		git config -f config.worktree core.worktree "$CORE_WT" &&
		git config --unset core.worktree &&
		git config extensions.worktreeConfig true &&
		git config core.worktree >actual &&
		echo "$CORE_WT" >expect &&
		test_cmp expect actual
	)
'

test_expect_success 'new worktree in submodule' '
	(
		cd addtest/submod &&
		git worktree add submod-elsewhere &&
		cd submod-elsewhere &&
		test_must_fail git config core.worktree
	)
'

test_expect_success 'new worktree in supermodule' '
	(
		cd addtest &&
		git commit -m initial &&
		git worktree add super-elsewhere &&
		cd super-elsewhere &&
		test_must_fail git config submodule.submode
	)
'

test_expect_success 'submodule add in the second worktree' '
	(
		cd addtest/super-elsewhere &&
		git submodule add -q "$submodurl" submod2 >actual &&
		test_must_be_empty actual &&
		echo "gitdir: ../../.git/worktrees/super-elsewhere/modules/submod2" >expect &&
		test_cmp expect submod2/.git &&
		(
			cd submod2 &&
			git config core.worktree >actual &&
			echo "../../../../../super-elsewhere/submod2" >expect &&
			test_cmp expect actual &&
			rm -f actual expect
		) &&
		git submodule init
	)
'

test_expect_success 'submodule.* in supermodule is per-worktree' '
	(
		cd addtest/super-elsewhere &&
		git config -f ../.git/worktrees/super-elsewhere/config.worktree submodule.submod2.url >actual &&
		echo "$submodurl" >expect &&
		test_cmp expect actual
	)
'

test_done
