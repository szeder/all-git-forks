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
  |           .-------cs60sb5----- d59np2z    |
  |          /                   /            |
  |  d8znjge0 ---a2dit76---b4crl15            |
  |                                           |
  +-------------------------------------------+

'

. ./test-lib.sh

test_expect_success 'blob and tree' '
	test_tick &&
	(
		for i in 0 1 2 3 4 5 6 7 8 9
		do
			echo $i
		done
		echo
		echo cbpkuqa
	) >bz01t33 &&

	# create one blob 1102198254
	git add bz01t33 &&

	# create one tree 1102198206
	git write-tree
'

test_expect_success 'warn ambiguity when no candidate matches type hint' '
	test_must_fail git rev-parse --verify 11021982^{commit} 2>actual &&
	grep "short SHA1 11021982 is ambiguous" actual
'

test_expect_success 'disambiguate tree-ish' '
	# feed tree-ish in an unambiguous way
	git rev-parse --verify 1102198206:bz01t33 &&

	# ambiguous at the object name level, but there is only one
	# such tree-ish (the other is a blob)
	git rev-parse --verify 11021982:bz01t33
'

test_expect_failure 'disambiguate blob' '
	sed -e "s/|$//" >patch <<-EOF &&
	diff --git a/frotz b/frotz
	index 11021982..19821102 100644
	--- a/frotz
	+++ b/frotz
	@@ -10,3 +10,4 @@
	 9
	 |
	 cbpkuqa
	+aphcwki
	EOF
	(
		GIT_INDEX_FILE=frotz &&
		export GIT_INDEX_FILE &&
		git apply --build-fake-ancestor frotz patch &&
		git cat-file blob :frotz >actual
	) &&
	test_cmp bz01t33 actual
'

test_expect_failure 'disambiguate tree' '
	commit=$(echo "ajv1v8s" | git commit-tree 11021982) &&
	test $(git rev-parse $commit^{tree}) = $(git rev-parse 1102198206)
'

test_expect_success 'first commit' '
	# create one commit 1102198268
	git commit -m d8znjge0
'

test_expect_success 'disambiguate commit-ish' '
	# feed commit-ish in an unambiguous way
	git rev-parse --verify 1102198268^{commit} &&

	# ambiguous at the object name level, but there is only one
	# such commit (the others are tree and blob)
	git rev-parse --verify 11021982^{commit} &&

	# likewise
	git rev-parse --verify 11021982^0
'

test_expect_failure 'disambiguate commit' '
	commit=$(echo "ankqihm" | git commit-tree 11021982 -p 11021982) &&
	test $(git rev-parse $commit^) = $(git rev-parse 1102198268)
'

test_expect_success 'log name1..name2 takes only commit-ishes on both ends' '
	git log 11021982..11021982 &&
	git log ..11021982 &&
	git log 11021982.. &&
	git log 11021982...11021982 &&
	git log ...11021982 &&
	git log 11021982...
'

test_expect_failure 'rev-parse name1..name2 takes only commit-ishes on both ends' '
	git rev-parse 11021982..11021982 &&
	git rev-parse ..11021982 &&
	git rev-parse 11021982..
'

test_expect_success 'git log takes only commit-ish' '
	git log 11021982
'

test_expect_failure 'git reset takes only commit-ish' '
	git reset 11021982
'

test_expect_success 'first tag' '
	# create one tag 1102198256
	git tag -a -m d3g97ek v1.0.0
'

test_expect_failure 'two semi-ambiguous commit-ish' '
	# Once the parser becomes ultra-smart, it could notice that
	# 110282 before ^{commit} name many different objects, but
	# that only two (HEAD and v1.0.0 tag) can be peeled to commit,
	# and that peeling them down to commit yield the same commit
	# without ambiguity.
	git rev-parse --verify 110282^{commit} &&

	# likewise
	git log 11021982..11021982 &&
	git log ..11021982 &&
	git log 11021982.. &&
	git log 11021982...11021982 &&
	git log ...11021982 &&
	git log 11021982...
'

test_expect_failure 'three semi-ambiguous tree-ish' '
	# Likewise for tree-ish.  HEAD, v1.0.0 and HEAD^{tree} share
	# the prefix but peeling them to tree yields the same thing
	git rev-parse --verify 110282^{tree}
'

test_expect_success 'parse describe name' '
	# feed an unambiguous describe name
	git rev-parse --verify v1.0.0-0-g1102198268 &&

	# ambiguous at the object name level, but there is only one
	# such commit (others are blob, tree and tag)
	git rev-parse --verify v1.0.0-0-g11021982
'

test_expect_success 'more history' '
	# commit 110219822
	git mv bz01t33 b90au8x &&
	echo bdc8xi8 >>b90au8x &&
	git add b90au8x &&

	test_tick &&
	git commit -m a2dit76 &&

	# commit 110219828
	git mv b90au8x djintr2 &&
	echo a2a49zt >>djintr2 &&
	git add djintr2 &&

	test_tick &&
	git commit -m b4crl15 &&

	# commit 11021982d
	git checkout v1.0.0^0 &&
	git mv bz01t33 ddp2qt1 &&

	for i in bdc8xi8 a2a49zt c0l9aeu
	do
		echo $i
	done >>ddp2qt1 &&
	git add ddp2qt1 &&

	test_tick &&
	git commit -m cs60sb5 &&
	side=$(git rev-parse HEAD) &&

	# commit 11021982a
	git checkout master &&

	# If you use recursive, merge will fail and you will need to
	# clean up bz01t33 as well.  If you use resolve, merge will
	# succeed.
	test_might_fail git merge --no-commit -s resolve $side &&
	git rm -f ddp2qt1 djintr2 &&
	test_might_fail git rm -f bz01t33 &&
	(
		git cat-file blob $side:ddp2qt1
		echo d4o8f4f
	) >c3q9963 &&
	git add c3q9963 &&

	test_tick &&
	git commit -m d59np2z

'

test_expect_failure 'parse describe name taking advantage of generation' '
	# ambiguous at the object name level, but there is only one
	# such commit at generation 0
	git rev-parse --verify v1.0.0-0-g11021982 &&

	# likewise for generation 2 and 4
	git rev-parse --verify v1.0.0-2-g11021982 &&
	git rev-parse --verify v1.0.0-4-g11021982
'

# Note: because rev-parse does not even try to disambiguate based on
# the generation number, this test currently succeeds for a wrong
# reason.  When it learns to use the generation number, the previous
# test should succeed, and also this test should fail because the
# describe name used in the test with generation number can name two
# commits.  Make sure that such a future enhancement does not randomly
# pick one.
test_expect_success 'parse describe name not ignoring ambiguity' '
	# ambiguous at the object name level, and there are two such
	# commits at generation 1
	test_must_fail git rev-parse --verify v1.0.0-1-g11021982
'

test_expect_success 'ambiguous commit-ish' '
	# Now there are many commits that begin with the
	# common prefix, none of these should pick one at
	# random.  They all should result in ambiguity errors.
	test_must_fail git rev-parse --verify 110282^{commit} &&

	# likewise
	test_must_fail git log 11021982..11021982 &&
	test_must_fail git log ..11021982 &&
	test_must_fail git log 11021982.. &&
	test_must_fail git log 11021982...11021982 &&
	test_must_fail git log ...11021982 &&
	test_must_fail git log 11021982...
'

test_done
