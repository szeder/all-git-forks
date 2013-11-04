#!/bin/sh

test_description='exercise basic bitmap functionality'
. ./test-lib.sh

test_expect_success 'setup repo with moderate-sized history' '
	for i in $(test_seq 1 10); do
		test_commit $i
	done &&
	git checkout -b other HEAD~5 &&
	for i in `test_seq 1 10`; do
		test_commit side-$i
	done &&
	git checkout master &&
	blob=$(echo tagged-blob | git hash-object -w --stdin) &&
	git tag tagged-blob $blob &&
	git config pack.writebitmaps true
'

test_expect_success 'full repack creates bitmaps' '
	git repack -ad &&
	ls .git/objects/pack/ | grep bitmap >output &&
	test_line_count = 1 output
'

test_expect_success 'rev-list --test-bitmap verifies bitmaps' '
	git rev-list --test-bitmap HEAD
'

rev_list_tests() {
	state=$1

	test_expect_success "counting commits via bitmap ($state)" '
		git rev-list --count HEAD >expect &&
		git rev-list --use-bitmap-index --count HEAD >actual &&
		test_cmp expect actual
	'

	test_expect_success "counting partial commits via bitmap ($state)" '
		git rev-list --count HEAD~5..HEAD >expect &&
		git rev-list --use-bitmap-index --count HEAD~5..HEAD >actual &&
		test_cmp expect actual
	'

	test_expect_success "counting non-linear history ($state)" '
		git rev-list --count other...master >expect &&
		git rev-list --use-bitmap-index --count other...master >actual &&
		test_cmp expect actual
	'

	test_expect_success "enumerate --objects ($state)" '
		git rev-list --objects --use-bitmap-index HEAD >tmp &&
		cut -d" " -f1 <tmp >tmp2 &&
		sort <tmp2 >actual &&
		git rev-list --objects HEAD >tmp &&
		cut -d" " -f1 <tmp >tmp2 &&
		sort <tmp2 >expect &&
		test_cmp expect actual
	'

	test_expect_success "bitmap --objects handles non-commit objects ($state)" '
		git rev-list --objects --use-bitmap-index HEAD tagged-blob >actual &&
		grep $blob actual
	'
}

rev_list_tests 'full bitmap'

test_expect_success 'clone from bitmapped repository' '
	git clone --no-local --bare . clone.git &&
	git rev-parse HEAD >expect &&
	git --git-dir=clone.git rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'setup further non-bitmapped commits' '
	for i in `test_seq 1 10`; do
		test_commit further-$i
	done
'

rev_list_tests 'partial bitmap'

test_expect_success 'fetch (partial bitmap)' '
	git --git-dir=clone.git fetch origin master:master &&
	git rev-parse HEAD >expect &&
	git --git-dir=clone.git rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_expect_success 'incremental repack cannot create bitmaps' '
	test_commit more-1 &&
	test_must_fail git repack -d
'

test_expect_success 'incremental repack can disable bitmaps' '
	test_commit more-2 &&
	git repack -d --no-write-bitmap-index
'

test_expect_success 'full repack, reusing previous bitmaps' '
	git repack -ad &&
	ls .git/objects/pack/ | grep bitmap >output &&
	test_line_count = 1 output
'

test_expect_success 'fetch (full bitmap)' '
	git --git-dir=clone.git fetch origin master:master &&
	git rev-parse HEAD >expect &&
	git --git-dir=clone.git rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_done
