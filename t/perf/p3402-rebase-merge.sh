#!/bin/sh

test_description="Tests rebase performance with merge backend"

. ./perf-lib.sh

test_perf_default_repo
test_checkout_worktree

# Setup a topic branch with 50 commits
test_expect_success 'setup topic branch' '
	git checkout -b perf-topic-branch master &&
	for i in $(test_seq 50); do
		test_commit perf-$i file
	done &&
	git tag perf-topic-branch-initial
'

test_perf 'rebase -m --onto master^' '
	git checkout perf-topic-branch &&
	git reset --hard perf-topic-branch-initial &&
	git rebase -m --onto master^ master
'

test_done
