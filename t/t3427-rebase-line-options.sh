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
	test_commit Commit2 &&
	git checkout -b branch Commit1 &&
	test_commit Commit2_ Commit2.t
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

test_msg_author () {
	set_fake_editor &&
	FAKE_LINES="1 $1 2" git rebase -i HEAD~2 &&
	commit_message HEAD >actual.msg &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.msg actual.msg &&
	test_cmp expected.author actual.author
}

test_msg_author_misspelled () {
	set_cat_todo_editor &&
	test_must_fail git rebase -i HEAD^ >todo &&
	set_fake_editor &&
	test_must_fail env FAKE_LINES="1 $1-misspelled 2" git rebase -i HEAD~2 &&
	set_fixed_todo_editor "$(pwd)"/todo &&
	FAKE_LINES="$1 1" git rebase --edit-todo &&
	git rebase --continue &&
	commit_message HEAD >actual.msg &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.msg actual.msg &&
	test_cmp expected.author actual.author
}

test_msg_author_conflicted () {
	set_fake_editor &&
	test_must_fail env FAKE_LINES="$1 1" git rebase -i master &&
	git checkout --theirs Commit2.t &&
	git add Commit2.t &&
	git rebase --continue &&
	commit_message HEAD >actual.msg &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.msg actual.msg &&
	test_cmp expected.author actual.author
}

test_expect_success 'Misspelled pick --signoff' '
	git checkout -b misspelled-pick--signoff master &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	commit_authorship HEAD >expected.author &&
	test_msg_author_misspelled pick_--signoff
'

test_expect_success 'Conflicted pick --signoff' '
	git checkout -b conflicted-pick--signoff branch &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	commit_authorship HEAD >expected.author &&
	test_msg_author_conflicted pick_--signoff
'

test_expect_success 'pick --signoff' '
	git checkout -b pick--signoff master &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	commit_authorship HEAD >expected.author &&
	test_msg_author pick_--signoff
'

test_expect_success 'reword --signoff' '
	git checkout -b reword--signoff master &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	commit_authorship HEAD >expected.author &&
	test_msg_author reword_--signoff
'

test_expect_success 'edit --signoff' '
	git checkout -b edit--signoff master &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	commit_authorship HEAD >expected.author &&
	set_fake_editor &&
	FAKE_LINES="1 edit_--signoff 2" git rebase -i HEAD~2 &&
	git rebase --continue &&
	commit_message HEAD >actual.msg &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.msg actual.msg &&
	test_cmp expected.author actual.author
'

test_expect_success 'Misspelled pick --reset-author' '
	git checkout -b misspelled-pick--reset-author master &&
	commit_message HEAD >expected.msg &&
	test_tick &&
	authorship >expected.author &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	test_msg_author_misspelled pick_--reset-author
'

test_expect_success 'Conflicted pick --reset-author' '
	git checkout -b conflicted-pick--reset-author branch &&
	commit_message HEAD >expected.msg &&
	test_tick &&
	authorship >expected.author &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	test_msg_author_conflicted pick_--reset-author
'

test_expect_success 'pick --reset-author' '
	git checkout -b pick--reset-author master &&
	commit_message HEAD >expected.msg &&
	test_tick &&
	authorship >expected.author &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	test_msg_author pick_--reset-author
'

test_expect_success 'pick --reset-author --signoff' '
	git checkout -b pick--reset-author--signoff master &&
	cat >expected.msg <<-EOF &&
	$(commit_message HEAD)

	Signed-off-by: C O Mitter <committer@example.com>
	EOF
	test_tick &&
	authorship >expected.author &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	test_msg_author pick_--reset-author_--signoff
'

test_expect_success 'reword --reset-author' '
	git checkout -b reword--reset-author master &&
	commit_message HEAD >expected.msg &&
	test_tick &&
	authorship >expected.author &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	test_msg_author reword_--reset-author
'

test_expect_success 'edit --reset-author' '
	git checkout -b edit--reset-author master &&
	commit_message HEAD >expected.msg &&
	commit_authorship HEAD >original.author &&
	test_diff_file expected.author original.author &&
	set_fake_editor &&
	FAKE_LINES="1 edit_--reset-author 2" git rebase -i HEAD~2 &&
	>Commit2.t &&
	git add Commit2.t &&
	test_tick &&
	authorship >expected.author &&
	git rebase --continue &&
	commit_message HEAD >actual.msg &&
	commit_authorship HEAD >actual.author &&
	test_cmp expected.msg actual.msg &&
	test_cmp expected.author actual.author
'

test_done
