#!/bin/sh

test_description='detect unwritable repository and fail correctly'

. ./test-lib.sh

test_expect_success setup '

	>file &&
	git add file &&
	git commit -m initial &&
	echo >file &&
	git add file

'

test_expect_success 'write-tree should notice unwritable repository' '

	(
		chmod a-w .git/objects
		test_must_fail git write-tree
	)
	status=$?
	chmod 775 .git/objects
	(exit $status)

'

test_expect_success 'commit should notice unwritable repository' '

	(
		chmod a-w .git/objects
		test_must_fail git commit -m second
	)
	status=$?
	chmod 775 .git/objects
	(exit $status)

'

test_expect_success 'update-index should notice unwritable repository' '

	(
		echo a >file &&
		chmod a-w .git/objects
		test_must_fail git update-index file
	)
	status=$?
	chmod 775 .git/objects
	(exit $status)

'

test_expect_success 'add should notice unwritable repository' '

	(
		echo b >file &&
		chmod a-w .git/objects
		test_must_fail git add file
	)
	status=$?
	chmod 775 .git/objects
	(exit $status)

'

test_done
