#!/bin/sh

test_description='no prune detached head without reflog'
. ./test-lib.sh

test_expect_success 'make repo' '
	git config core.logAllRefUpdates false
	git commit --allow-empty -m commit1 &&
	git commit --allow-empty -m commit2 &&
	git checkout  --detach master &&
	git commit --allow-empty -m commit3
'

test_expect_success 'prune does not delete anything' '
	git prune -n >prune_actual &&
	: >prune_expected &&
	test_cmp prune_expected prune_actual'

test_done
