#!/bin/sh

test_description='git rebase -i with line options'

. ./test-lib.sh

. "$TEST_DIRECTORY"/lib-rebase.sh

commit_message () {
	git cat-file commit "$1" | sed '1,/^$/d'
}

commit_authorship () {
	git cat-file commit "$1" | sed -n '/^$/q;/^author /p'
}

authorship () {
	echo "author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE"
}

test_diff_file () {
	if cmp "$1" "$2" >/dev/null
	then
		echo "'$1' and '$2' are the same"
		return 1
	fi
}

test_expect_success 'Set up repository' '
	test_commit Initial &&
	test_commit Commit1 &&
	test_commit Commit2
'

test_expect_success 'pick --signoff' '
	cat >expected.msg <<EOF
Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="pick_--signoff 1" git rebase -i Commit2^ &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_success 'Misspelled pick --signoff' '
	cat >expected.msg <<EOF
Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_when_finished reset_rebase Commit2 &&
	set_cat_todo_editor &&
	test_must_fail git rebase -i Commit2^ >todo &&
	set_fake_editor &&
	test_must_fail env FAKE_LINES="pick_--sign 1" git rebase -i Commit2^ &&
	set_fixed_todo_editor "$(pwd)"/todo &&
	env FAKE_LINES="pick_--signoff 1" git rebase --edit-todo &&
	git rebase --continue &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_success 'reword --signoff' '
	cat >expected.msg <<EOF
Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="reword_--signoff 1" git rebase -i Commit2^ &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_failure 'squash --signoff' '
	cat >expected.msg <<EOF
Commit1

Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="1 squash_--signoff 2" git rebase -i Commit1^ &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_failure 'fixup --signoff' '
	cat >expected.msg <<EOF
Commit1

Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="1 fixup_--signoff 2" git rebase -i Commit1^ &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_success 'pick --reset-author' '
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit2 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="pick_--reset-author 1" git rebase -i Commit2^ &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_expect_success 'Misspelled pick --reset-author' '
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit2 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_cat_todo_editor &&
	test_must_fail git rebase -i Commit2^ >todo &&
	set_fake_editor &&
	test_must_fail env FAKE_LINES="pick_--ignore-date 1" git rebase -i Commit2^ &&
	set_fixed_todo_editor "$(pwd)"/todo &&
	FAKE_LINES="pick_--reset-author 1" git rebase --edit-todo &&
	git rebase --continue &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_expect_success 'pick --reset-author --signoff' '
	cat >expected.msg <<EOF
Commit2

Signed-off-by: C O Mitter <committer@example.com>
EOF
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit2 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="pick_--reset-author_--signoff 1" git rebase -i Commit2^ &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author &&
	commit_message HEAD >actual.msg &&
	test_cmp expected.msg actual.msg
'

test_expect_success 'reword --reset-author' '
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit2 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="reword_--reset-author 1" git rebase -i Commit2^ &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_expect_failure 'squash --reset-author' '
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit1 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="1 squash_--reset-author 2" git rebase -i Commit1^ &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_expect_failure 'fixup --reset-author' '
	test_tick &&
	authorship >expected.author &&
	commit_authorship Commit1 >original.author
	test_diff_file expected.author original.author &&
	test_when_finished reset_rebase Commit2 &&
	set_fake_editor &&
	FAKE_LINES="1 fixup_--reset-author 2" git rebase -i Commit1^ &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_done
