#!/bin/sh
#
# Copyright (c) 2008 Nguyễn Thái Ngọc Duy
#

test_description='git update-index no-checkout bits (a.k.a sparse checkout)'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir sub &&
	touch 1 2 sub/1 sub/2 &&
	git add 1 2 sub/1 sub/2
'

test_expect_success 'index is at version 2' '
	test "$(test-index-version < .git/index)" = 2
'

test_expect_success 'update-index --no-checkout' '
	git update-index --no-checkout 1 sub/1 &&
	test -z "$(git ls-files --sparse|grep 1)"'

test_expect_success 'index is at version 3 after having some no-checkout entries' '
	test "$(test-index-version < .git/index)" = 3
'

test_expect_success 'update-index --checkout' '
	git update-index --checkout 1 sub/1 &&
	test "$(git ls-files)" = "$(git ls-files --sparse)"'

test_expect_success 'index version is back to 2 when there is no no-checkout entry' '
	test "$(test-index-version < .git/index)" = 2
'

test_done
