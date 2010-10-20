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

test_help success branch
test_help success checkout-index
test_help success commit
test_help success gc
test_help success ls-files
test_help success merge
test_help success update-index
test_help success upload-archive

test_done
