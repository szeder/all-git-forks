#!/bin/sh
#
# Copyright (c) 2010 Marc Branchaud
#

test_description='git rebase -i --no-ff tests'

. ./test-lib.sh

. "$TEST_DIRECTORY"/lib-rebase.sh

set_fake_editor

test_expect_success setup '
	echo hello > hello &&
	git add hello &&
	git commit -m "hello" &&

	echo world >> hello &&
	git commit -a -m "hello world" &&

	echo goodbye >> hello &&
	git commit -a -m "goodbye" &&

	git tag old_head
	'
# Pause to ensure that the cherry-picked commits have a different
# timestamp.
sleep 1

test_expect_success 'rebase -i --no-ff' '
	git rebase -i --no-ff HEAD~2 &&
	test ! $(git rev-parse HEAD) = $(git rev-parse old_head)
	'

test_done
