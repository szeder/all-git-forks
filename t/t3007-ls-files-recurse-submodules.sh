#!/bin/sh

test_description='Test ls-files recurse-submodules feature

This test verifies the recurse-submodules feature correctly lists files from
submodules.
'

. ./test-lib.sh

test_expect_success 'setup directory structure and submodules' '
	echo a >a &&
	mkdir b &&
	echo b >b/b &&
	git add a b &&
	git commit -m "add a and b" &&
	mkdir submodule &&
	(
		cd submodule &&
		git init &&
		echo c >c &&
		git add c &&
		git commit -m "add c"
	) &&
	git submodule add ./submodule &&
	git commit -m "added submodule"
'

cat >expect <<EOF
.gitmodules
a
b/b
submodule/c
EOF

test_expect_success 'ls-files correctly outputs files in submodule' '
	git ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-files does not output files not added to a repo' '
	echo a >not_added &&
	echo b >b/not_added &&
	(
		cd submodule &&
		echo c >not_added
	) &&
	git ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

cat >expect <<EOF
.gitmodules
a
b/b
submodule/.gitmodules
submodule/c
submodule/subsub/d
EOF

test_expect_success 'ls-files recurses more than 1 level' '
	(
		cd submodule &&
		mkdir subsub &&
		(
			cd subsub &&
			git init &&
			echo d >d &&
			git add d &&
			git commit -m "add d"
		) &&
		git submodule add ./subsub &&
		git commit -m "added subsub"
	) &&
	git ls-files --recurse-submodules >actual &&
	test_cmp expect actual
'

cat >expect_error <<EOF
fatal: ls-files --recurse-submodules does not support path arguments
EOF

test_expect_success 'error when using path arguments' '
	test_must_fail git ls-files --recurse-submodules b 2>actual &&
	test_cmp expect_error actual
'

cat >expect_error <<EOF
fatal: ls-files --recurse-submodules can only be used in --cached mode
EOF

test_expect_success 'error when using different modes' '
	for opt in {v,t}; do
		test_must_fail git ls-files --recurse-submodules -$opt 2>actual &&
		test_cmp expect_error actual
	done &&
	for opt in {deleted,modified,others,ignored,stage,killed,unmerged,eol}; do
		test_must_fail git ls-files --recurse-submodules --$opt 2>actual &&
		test_cmp expect_error actual
	done
'

test_done
