#!/bin/sh

test_description='Test adding/editing of empty notes'
. ./test-lib.sh

write_script fake_editor <<\EOF
	echo "$MSG" >"$1"
EOF
GIT_EDITOR=./fake_editor
export GIT_EDITOR

test_expect_success 'setup' '
	test_commit one &&
	git log -1 >expect_missing &&
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
done

test_done
