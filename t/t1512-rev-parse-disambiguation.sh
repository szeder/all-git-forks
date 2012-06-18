#!/bin/sh

test_description='object name disambiguation

Create blobs, trees, commits and a tag that all share the same
prefix, and make sure "git rev-parse" can take advantage of
type information to disambiguate short object names that are
not necessarily unique.

The final history used in the test has five commits, with the bottom
one tagged as v1.0.0.  They all have one regular file each.

  +-------------------------------------------+
  |                                           |
  |           .--------7sye6------ qng9a      |
  |          /                   /            |
  |     kjz37 --- 4h98d --- r34k1             |
  |                                           |
  +-------------------------------------------+

'

. ./test-lib.sh

test_expect_success 'blob and tree' '
	(
		for i in 0 1 2 3 4 5 6 7 8 9
		do
			echo $i
		done
		echo
		echo 1nkn9
	) >jc4qa &&

	# create one blob 1102825e...
	git add jc4qa &&

	# create one tree 11028247...
	git write-tree
'

test_expect_failure 'disambiguate tree-ish' '
	# feed tree-ish in an unambiguous way
	git rev-parse --verify 1102824:jc4qa &&

	# ambiguous at the object name level, but
	# there is only one such tree
	git rev-parse --verify 110282:jc4qa
'

test_expect_success 'first commit' '
	# create one commit 110282b1...
	test_tick &&
	git commit -m kjz37
'

test_expect_failure 'disambiguate commit-ish' '
	# feed commit-ish in an unambiguous way
	git rev-parse --verify 110282b^{commit} &&

	# ambiguous at the object name level, but
	# there is only one such commit
	git rev-parse --verify 110282^{commit} &&

	# likewise
	git rev-parse --verify 110282^0
'

test_expect_success 'first tag' '
	# create one tag 1102823f...
	git tag -a -m 9gcp1 v1.0.0
'

test_expect_success 'should notice two commit-ish' '
	# both HEAD and v1.0.0 are commit-ish
	test_must_fail git rev-parse --verify 110282^{commit}
'

test_expect_success 'parse describe name' '
	# feed an unambiguous describe name
	git rev-parse --verify v1.0.0-0-g110282b &&

	# ambiguous at the object name level, but
	# there is only one such commit
	git rev-parse --verify v1.0.0-0-g110282
'

test_expect_success 'more history' '
	# commit 110282c2...
	git mv jc4qa 7gc4p &&
	echo mdxn2 >>7gc4p &&
	git add 7gc4p &&

	test_tick &&
	git commit -m 4h98d &&

	# commit 1102823a...
	git mv 7gc4p aov31 &&
	echo o0sc0 >>aov31 &&
	git add aov31 &&

	test_tick &&
	git commit -m r34k1 &&

	# commit 110282c6...
	git checkout v1.0.0^0 &&
	git mv jc4qa bvuha &&

	for i in mdxn2 o0sc0 bxur
	do
		echo $i
	done >>bvuha &&
	git add bvuha &&

	test_tick &&
	git commit -m 7sye6 &&
	side=$(git rev-parse HEAD) &&

	# commit 11028259...
	git checkout master &&
	test_must_fail git merge --no-commit - &&

	git rm -f aov31 bvuha jc4qa &&
	(
		git cat-file blob $side:bvuha
		echo jrd3a
	) >orti0 &&
	git add orti0 &&

	test_tick &&
	git commit -m qng9a

'

test_expect_failure 'parse describe name taking advantage of generation' '
	# ambiguous at the object name level, but
	# there is only one such commit at generation 0
	git rev-parse --verify v1.0.0-0-g110282 &&

	# likewise for generation 2 and 4
	git rev-parse --verify v1.0.0-2-g110282 &&
	git rev-parse --verify v1.0.0-4-g110282
'

# Note: this testcurrently succeeds for a wrong reason, namely,
# rev-parse does not even try to disambiguate based on generation
# number. This test is here to make sure such a future enhancement
# does not randomly pick one.
test_expect_success 'parse describe name not ignoring ambiguity' '
	# ambiguous at the object name level, and
	# there are two such commits at generation 1
	test_must_fail git rev-parse --verify v1.0.0-1-g110282
'

test_done
