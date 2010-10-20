#!/bin/sh

test_description='tests that git foo -h should work even in potentially broken repos'

. ./test-lib.sh

test_help() {
	test_expect_"$1" "$2 -h" "
		GIT_TRACE=\"`pwd`\"/$2.log test_must_fail git $2 -h &&
		test \$exit_code = 129 &&
		! grep 'defaults to' $2.log
	"
}

test_help failure branch
test_help failure checkout-index
test_help failure commit
test_help failure gc
test_help failure ls-files
test_help failure merge
test_help failure update-index
test_help failure upload-archive

test_done
