#!/bin/sh
#
# Copyright (c) 2016, Twitter, Inc
#

test_description='git-index-helper

Testing git index-helper
'

. ./test-lib.sh

test -z "$HAVE_SHM" && {
	skip_all='skipping index-helper tests: no SHM'
	test_done
}

test_expect_success 'index-helper smoke test' '
	git index-helper --exit-after 1 &&
	test_path_is_missing .git/index-helper.path
'

test_expect_success 'index-helper creates usable path file and can be killed' '
	test_when_finished "git index-helper --kill" &&
	test_path_is_missing .git/index-helper.path &&
	git index-helper --detach &&
	test -L .git/index-helper.path &&
	sock="$(readlink .git/index-helper.path)" &&
	test -S "$sock" &&
	dir=$(dirname "$sock") &&
	ls -ld "$dir" | grep ^drwx...--- &&
	git index-helper --kill &&
	test_path_is_missing .git/index-helper.path
'

test_expect_success 'index-helper will not start if already running' '
	test_when_finished "git index-helper --kill" &&
	git index-helper --detach &&
	test -L .git/index-helper.path &&
	test_must_fail git index-helper 2>err &&
	test -L .git/index-helper.path &&
	grep "Already running" err
'

test_expect_success 'index-helper is quiet with --autorun' '
	test_when_finished "git index-helper --kill" &&
	git index-helper --kill &&
	git index-helper --detach &&
	test -L .git/index-helper.path &&
	git index-helper --autorun
'

test_done
