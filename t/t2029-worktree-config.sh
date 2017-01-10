#!/bin/sh

test_description="config file in multi worktree"

. ./test-lib.sh

cmp_config() {
	if [ "$1" = "-C" ]; then
		shift &&
		GD="-C $1" &&
		shift
	else
		GD=
	fi &&
	echo "$1" >expected &&
	shift &&
	git $GD config "$@" >actual &&
	test_cmp expected actual
}

test_expect_success 'setup' '
	test_commit start &&
	git config --worktree per.worktree is-ok &&
	git worktree add wt1 &&
	git worktree add wt2 &&
	git config --worktree per.worktree is-ok &&
	cmp_config true extensions.worktreeConfig
'

test_expect_success 'config is shared as before' '
	git config this.is shared &&
	cmp_config shared this.is &&
	cmp_config -C wt1 shared this.is &&
	cmp_config -C wt2 shared this.is
'

test_expect_success 'config is shared (set from another worktree)' '
	git -C wt1 config that.is also-shared &&
	cmp_config also-shared that.is &&
	cmp_config -C wt1 also-shared that.is &&
	cmp_config -C wt2 also-shared that.is
'

test_expect_success 'config private to main worktree' '
	git config --worktree this.is for-main &&
	cmp_config for-main this.is &&
	cmp_config -C wt1 shared this.is &&
	cmp_config -C wt2 shared this.is
'

test_expect_success 'config private to linked worktree' '
	git -C wt1 config --worktree this.is for-wt1 &&
	cmp_config for-main this.is &&
	cmp_config -C wt1 for-wt1 this.is &&
	cmp_config -C wt2 shared this.is
'

test_expect_success 'core.bare no longer for main only' '
	git config core.bare true &&
	cmp_config true core.bare &&
	cmp_config -C wt1 true core.bare &&
	cmp_config -C wt2 true core.bare &&
	git config --unset core.bare
'

test_expect_success 'config.worktree no longer read without extension' '
	git config --unset extensions.worktreeConfig &&
	cmp_config shared this.is &&
	cmp_config -C wt1 shared this.is &&
	cmp_config -C wt2 shared this.is
'

test_expect_success 'config --worktree migrate core.bare and core.worktree' '
	git config core.bare true &&
	git config --worktree foo.bar true &&
	cmp_config true extensions.worktreeConfig &&
	cmp_config true foo.bar &&
	cmp_config true core.bare &&
	! git -C wt1 config core.bare
'

test_done
