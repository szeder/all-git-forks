#!/bin/sh

test_description='rev-list/rev-parse rev^- parsing'

. ./test-lib.sh

test_expect_success setup '
	test_commit one &&
	test_commit two &&
	test_commit three &&

	# Merge in a branch for testing ^-
	git checkout -b branch &&
	git checkout HEAD^^ &&
	git merge -m merge --no-edit --no-ff branch &&
	git checkout -b merge
'

# The merged branch has 2 commits + the merge
test_expect_success 'rev-list --count merge^- = merge^..merge' '
	git rev-list --count merge^..merge >expect &&
	echo 3 >actual &&
	test_cmp expect actual
'

# All rev-parse tests

test_expect_success 'rev-parse merge^- = merge^..merge' '
	git rev-parse merge^..merge >expect &&
	git rev-parse merge^- >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-parse merge^-1 = merge^..merge' '
	git rev-parse merge^1..merge >expect &&
	git rev-parse merge^-1 >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-parse merge^-2 = merge^2..merge' '
	git rev-parse merge^2..merge >expect &&
	git rev-parse merge^-2 >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-parse merge^-0' '
	test_must_fail git rev-parse merge^-0
'

test_expect_success 'rev-parse merge^-3' '
	test_must_fail git rev-parse merge^-3
'

test_expect_success 'rev-parse merge^-^' '
	test_must_fail git rev-parse merge^-^
'

# All rev-list tests (should be mostly the same as rev-parse)

test_expect_success 'rev-list merge^- = merge^..merge' '
	git rev-list merge^..merge >expect &&
	git rev-list merge^- >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-list merge^-1 = merge^1..merge' '
	git rev-list merge^1..merge >expect &&
	git rev-list merge^-1 >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-list merge^-2 = merge^2..merge' '
	git rev-list merge^2..merge >expect &&
	git rev-list merge^-2 >actual &&
	test_cmp expect actual
'

test_expect_success 'rev-list merge^-0' '
	test_must_fail git rev-list merge^-0
'

test_expect_success 'rev-list merge^-3' '
	test_must_fail git rev-list merge^-3
'

test_expect_success 'rev-list merge^-^' '
	test_must_fail git rev-list merge^-^
'

test_done
