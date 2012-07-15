#!/bin/sh

test_description='test that fast-import handles bogus input correctly'
. ./test-lib.sh

# A few shorthands to make writing sample input easier
counter=0
mark() {
	counter=$(( $counter + 1)) &&
	echo "mark :$counter"
}

blob() {
	echo blob &&
	mark &&
	cat <<-\EOF
	data 4
	foo

	EOF
}

ident() {
	test_tick &&
	echo "author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE"
	echo "committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE"
}

commit() {
	echo "commit refs/heads/master" &&
	mark &&
	ident &&
	cat <<-\EOF
	data 8
	message
	EOF
}

root() {
	blob &&
	m=$counter &&
	echo "reset refs/heads/master" &&
	commit &&
	echo "M 100644 :$m file" &&
	echo
}

test_expect_success 'deleting a nonexistent file is an error' '
	{
		root &&
		commit &&
		echo "D does-not-exist"
	} >input &&
	test_must_fail git fast-import --force <input
'

test_expect_success 'renaming a deleted file is an error' '
	{
		root &&
		commit &&
		echo "D file" &&
		echo "R file dest"
	} >input &&
	test_must_fail git fast-import --force <input
'

test_expect_success 'deleting a renamed file is an error' '
	{
		root &&
		commit &&
		echo "R file dest" &&
		echo "D file"
	} >input &&
	test_must_fail git fast-import --force <input
'

test_done
