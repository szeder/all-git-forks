#!/bin/sh

test_description='git rebase -i with line options'

. ./test-lib.sh

. "$TEST_DIRECTORY"/lib-rebase.sh

test_expect_success 'Set up repository' '
	test_commit Initial &&
	test_commit Commit1 &&
	test_commit Commit2
'

test_expect_success 'Unknown option' '
	git checkout -b unknown-option master &&
	set_cat_todo_editor &&
	test_must_fail git rebase -i HEAD^ >todo &&
	set_fake_editor &&
	test_must_fail env FAKE_LINES="1 pick_--unknown-option 2" git rebase -i HEAD~2 &&
	set_fixed_todo_editor "$(pwd)"/todo &&
	git rebase --edit-todo &&
	git rebase --continue
'

test_done
