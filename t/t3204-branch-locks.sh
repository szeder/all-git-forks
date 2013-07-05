#!/bin/sh

test_description='git branch lock tests'

. ./test-lib.sh

test_expect_success 'prepare a trivial repository' '
	git commit --allow-empty -m "Initial" &&
	git commit --allow-empty -m "First"
'

test_expect_success 'delete a ref while loose ref file is locked' '
	git branch b1 master &&
	git for-each-ref >expected1 &&
	# Simulate another process holding the loose file lock:
	: >.git/refs/heads/b1.lock &&
	test_must_fail git branch -D b1 &&
	rm .git/refs/heads/b1.lock &&
	# Delete failed; reference values should be unchanged:
	git for-each-ref >actual1 &&
	test_cmp expected1 actual1
'

test_expect_failure 'delete a ref while packed-refs file is locked' '
	git branch b2 master &&
	# Pack current value of b2:
	git pack-refs --all &&
	# Overwrite packed value with a loose value:
	git branch -f b2 master^ &&
	git for-each-ref >expected2 &&
	# Simulate another process holding the packed-refs file lock:
	: >.git/packed-refs.lock &&
	test_must_fail git branch -D b2 &&
	rm .git/packed-refs.lock &&
	# Delete failed; reference values should be unchanged:
	git for-each-ref >actual2 &&
	test_cmp expected2 actual2
'

test_done
