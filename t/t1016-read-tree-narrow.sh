#!/bin/sh

test_description='read-tree in narrow repositories'

. ./test-lib.sh

test_expect_success setup '
	mkdir dir1 dir2 &&
	for i in a b c; do
		for j in "" dir1/ dir2/; do
			: >$j$i
		done
	done &&
	git add . &&
	git commit -m 1 &&
	git tag C1 &&

	echo outside >>a &&
	git add . &&
	git commit -m outside &&
	echo outside >>b &&
	echo inside >>dir1/b &&
	git add . &&
	git commit -m both &&
	echo inside >>dir1/c &&
	git add . &&
	git commit -m inside &&
	git tag C2 &&

	git reset --hard $C1 &&
	echo OUTSIDE >>a &&
	git add . &&
	git commit -m outside &&
	echo OUTSIDE >>b &&
	echo INSIDE >>dir1/b &&
	git add . &&
	git commit -m both &&
	echo INSIDE >>dir1/c &&
	git add . &&
	git commit -m inside &&
	git tag C3
'

test_expect_success 'read-tree C1' '
	echo dir1 >.git/narrow &&
	rm .git/index &&
	git read-tree C1 &&
	git ls-files --stage >result &&
	cat >expected <<-\EOF &&
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	dir1/a
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	dir1/b
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	dir1/c
EOF
	test_cmp expected result
'

test_expect_success 'read-tree C2 C2^ (no changes outside)' '
	rm .git/index &&
	git read-tree C2 C2^ &&
	git ls-files --stage >result &&
	cat >expected <<-\EOF &&
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	dir1/a
100644 5be24b7e8f4ff445fb089b101bb4f0f4909d84d5 0	dir1/b
100644 e69de29bb2d1d6434b8b29ae775ad8c2e48c5391 0	dir1/c
EOF
	test_cmp expected result &&
	git rev-parse --narrow-base >result &&
	git rev-parse C2^{tree} >expected &&
	test_cmp expected result
'

test_expect_success 'read-tree C2^ C2^^ (changes outside)' '
	rm .git/index &&
	test_must_fail git read-tree C2^ C2^^
'

test_expect_success 'read-tree -m C2^^ (changes outside wrt narrow base)' '
	git read-tree C2 C2^ &&
	test_must_fail git read-tree -m C2^^
'

test_expect_success 'fast forward (no changes outside)' '
	rm .git/index &&
	git read-tree C2^ &&
	git checkout . &&
	git read-tree -m C2^ C2 &&
	git rev-parse --narrow-base >result &&
	git rev-parse C2^{tree} >expected &&
	test_cmp expected result
'

test_expect_success 'fast forward (changes outside)' '
	rm .git/index &&
	git read-tree C2^^ &&
	git checkout . &&
	git read-tree -m C2^^ C2^ &&
	git rev-parse --narrow-base >result &&
	git rev-parse C2^^{tree} >expected &&
	test_cmp expected result
'

# FIXME three way merge

test_done
