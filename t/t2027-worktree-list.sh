#!/bin/sh

test_description='test git worktree list'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit init
'

test_expect_success '"list" all worktrees from main' '
	echo "$(git rev-parse --show-toplevel)       $(git rev-parse --short HEAD) [$(git symbolic-ref --short HEAD)]" >expect &&
	git worktree add --detach here master &&
	test_when_finished "rm -rf here && git worktree prune" &&
	echo "$(git -C here rev-parse --show-toplevel)  $(git rev-parse --short HEAD) (detached HEAD)" >>expect &&
	git worktree list >actual &&
	test_cmp expect actual
'

test_expect_success '"list" all worktrees from linked' '
	echo "$(git rev-parse --show-toplevel)       $(git rev-parse --short HEAD) [$(git symbolic-ref --short HEAD)]" >expect &&
	git worktree add --detach here master &&
	test_when_finished "rm -rf here && git worktree prune" &&
	echo "$(git -C here rev-parse --show-toplevel)  $(git rev-parse --short HEAD) (detached HEAD)" >>expect &&
	git -C here worktree list >actual &&
	test_cmp expect actual
'

test_expect_success '"list" all worktrees --porcelain' '
	echo "worktree $(git rev-parse --show-toplevel)" >expect &&
	echo "branch $(git symbolic-ref --short HEAD)" >>expect &&
	echo >>expect &&
	git worktree add --detach here master &&
	test_when_finished "rm -rf here && git worktree prune" &&
	echo "worktree $(git -C here rev-parse --show-toplevel)" >>expect &&
	echo "detached at $(git rev-parse --short HEAD)" >>expect &&
	echo >>expect &&
	git worktree list --porcelain >actual &&
	test_cmp expect actual
'

test_expect_success 'bare repo setup' '
	git init --bare bare1 &&
	echo "data" >file1 &&
	git add file1 &&
	git commit -m"File1: add data" &&
	git push bare1 master &&
	git reset --hard HEAD^
'

test_expect_success '"list" all worktrees from bare main' '
	git -C bare1 worktree add --detach ../there master &&
	test_when_finished "rm -rf there && git -C bare1 worktree prune" &&
	echo "$(pwd)/bare1  (bare)" >expect &&
	echo "$(git -C there rev-parse --show-toplevel)  $(git -C there rev-parse --short HEAD) (detached HEAD)" >>expect &&
	git -C bare1 worktree list >actual &&
	test_cmp expect actual
'

test_expect_success '"list" all worktrees --porcelain from bare main' '
	git -C bare1 worktree add --detach ../there master &&
	test_when_finished "rm -rf there && git -C bare1 worktree prune" &&
	echo "worktree $(pwd)/bare1" >expect &&
	echo "bare" >>expect &&
	echo >>expect &&
	echo "worktree $(git -C there rev-parse --show-toplevel)" >>expect &&
	echo "detached at $(git -C there rev-parse --short HEAD)" >>expect &&
	echo >>expect &&
	git -C bare1 worktree list --porcelain >actual &&
	test_cmp expect actual
'

test_expect_success '"list" all worktrees from linked with a bare main' '
	git -C bare1 worktree add --detach ../there master &&
	test_when_finished "rm -rf there && git -C bare1 worktree prune" &&
	echo "$(pwd)/bare1  (bare)" >expect &&
	echo "$(git -C there rev-parse --show-toplevel)  $(git -C there rev-parse --short HEAD) (detached HEAD)" >>expect &&
	git -C there worktree list >actual &&
	test_cmp expect actual
'

test_expect_success 'bare repo cleanup' '
	rm -rf bare1
'

test_done
