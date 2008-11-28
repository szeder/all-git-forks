#!/bin/sh

test_description="test ls-files in --sparse mode"

. ./test-lib.sh

test_ls_files() {
	T=../t3004/$1.expected
	shift
	test_expect_success "ls-files $*" "git ls-files $* > result && test_cmp $T result"
}

test_expect_success 'setup' '
	touch other orphaned no-checkout cached modified deleted &&
	git add orphaned no-checkout cached modified deleted &&
	git update-index --no-checkout orphaned no-checkout &&
	echo modified >> modified &&
	rm no-checkout deleted
'

test_ls_files cached
test_ls_files cached --cached
test_ls_files sparse-cached --sparse
test_ls_files sparse-cached --sparse --cached
test_ls_files no-checkout --no-checkout
test_ls_files no-checkout --sparse --no-checkout
test_ls_files orphaned --orphaned
test_ls_files orphaned --sparse --orphaned
test_ls_files orphaned-no-checkout -v --no-checkout --orphaned
test_ls_files orphaned-no-checkout -v --sparse --no-checkout --orphaned
test_ls_files deleted --deleted
test_ls_files deleted --sparse --deleted
test_ls_files modified --modified
test_ls_files modified --sparse --modified
test_ls_files others --others
test_ls_files others --sparse --others
test_ls_files everything -v --cached --deleted --modified --others
test_ls_files sparse-everything -v --cached --no-checkout --orphaned --deleted --modified --others

test_done