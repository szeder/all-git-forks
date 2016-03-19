#!/bin/sh
#
# Copyright (c) 2016, Twitter, Inc
#

test_description='git-index-helper

Testing git index-helper
'

. ./test-lib.sh

test_expect_success 'index-helper smoke test' '
	git index-helper --exit-after 1 &&
	test_path_is_missing .git/index-helper.pipe
'

test_expect_success 'index-helper creates usable pipe file and can be killed' '
	test_when_finished "git index-helper --kill" &&
	test_path_is_missing .git/index-helper.pipe &&
	git index-helper --detach &&
	test -p .git/index-helper.pipe &&
	git index-helper --kill &&
	test_path_is_missing .git/index-helper.pipe
'

test_expect_success 'index-helper will not start if already running' '
	test_when_finished "git index-helper --kill" &&
	git index-helper --detach &&
	test -p .git/index-helper.pipe &&
	test_must_fail git index-helper 2>err &&
	test -p .git/index-helper.pipe &&
	grep "Already running" err
'

test_expect_success 'index-helper is quiet with --autorun' '
	test_when_finished "git index-helper --kill" &&
	git index-helper --kill &&
	git index-helper --detach &&
	test -p .git/index-helper.pipe &&
	git index-helper --autorun
'

test_expect_success 'index-helper autorun works' '
	rm -f .git/index-helper.pipe &&
	git status &&
	test_path_is_missing .git/index-helper.pipe &&
	test_config indexhelper.autorun true &&
	git status &&
	test -p .git/index-helper.pipe &&
	git status 2>err &&
	test -p .git/index-helper.pipe &&
	! grep -q . err &&
	git index-helper --kill &&
	test_config indexhelper.autorun false &&
	git status &&
	test_path_is_missing .git/index-helper.pipe
'

test_done
