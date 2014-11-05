#!/bin/sh

test_description='Test adding/editing of empty notes with/without --allow-empty'
. ./test-lib.sh

write_script fake_editor <<\EOF
	echo "$MSG" >"$1"
EOF
GIT_EDITOR=./fake_editor
export GIT_EDITOR

test_expect_success 'setup' '
	test_commit one &&
	git log -1 >expect_missing &&
	cp expect_missing expect_empty &&
	echo >>expect_empty &&
	echo "Notes:" >>expect_empty &&
	empty_blob=$(git hash-object -w /dev/null)
'

cleanup_notes() {
	git update-ref -d refs/notes/commits
}

verify_missing() {
	git log -1 > actual &&
	test_cmp expect_missing actual &&
	! git notes list HEAD
}

verify_empty() {
	git log -1 > actual &&
	test_cmp expect_empty actual &&
	test "$(git notes list HEAD)" = "$empty_blob"
}

for cmd in \
	'add' \
	'add -F /dev/null' \
	'add -m ""' \
	'add -c "$empty_blob"' \
	'add -C "$empty_blob"' \
	'append' \
	'append -F /dev/null' \
	'append -m ""' \
	'append -c "$empty_blob"' \
	'append -C "$empty_blob"' \
	'edit'
do
	test_expect_success "'git notes $cmd' removes empty note" "
		cleanup_notes &&
		MSG= git notes $cmd &&
		verify_missing
	"

	test_expect_success "'git notes $cmd --allow-empty' stores empty note" "
		cleanup_notes &&
		MSG= git notes $cmd --allow-empty &&
		verify_empty
	"
done

test_done
