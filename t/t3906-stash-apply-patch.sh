#!/bin/sh

test_description='git stash apply --patch'
. ./lib-patch-mode.sh

test_expect_success 'setup' '
	test_commit one bar bar_head &&
	test_commit two foo foo_head &&
	set_state bar bar_work bar_index &&
	set_state foo foo_work foo_index &&
	save_head &&
	git stash
'

test_expect_success 'saying "n" does nothing' '
	git reset --hard &&
	(echo n; echo n) | git stash apply -p &&
	verify_state bar bar_head bar_head &&
	verify_state foo foo_head foo_head
'

# n/y will apply foo but not bar
test_expect_success 'git stash apply -p' '
	git reset --hard &&
	(echo n; echo y) | git stash apply -p &&
	verify_state bar bar_head bar_head &&
	verify_state foo foo_work foo_head
'

# we need two per file, for index and working tree
test_expect_success 'git stash apply -p --index' '
	git reset --hard &&
	(echo n; echo y; sleep 2; echo n; echo y) | git stash apply -p --index &&
	verify_state bar bar_head bar_head &&
	verify_state foo foo_work foo_index
'

test_expect_success 'none of this moved HEAD' '
	verify_saved_head
'

test_done
