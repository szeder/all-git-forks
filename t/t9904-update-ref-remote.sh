#!/bin/sh

test_description='git update-ref-remote'

. ./test-lib.sh

test_expect_success setup '
	>file &&
	git add file &&
	test_tick &&
	git commit -m initial &&
	git rev-parse HEAD > commit1 &&
        echo test >> file &&
	git add file &&
	git commit -m change1 &&
	git rev-parse HEAD > commit2 &&
	git checkout $(cat commit1) &&
	git remote add self "$(pwd)/.git"
'

test_expect_success 'update-ref-remote to referenced commit, passing oldval' '
	git update-ref-remote self refs/heads/master $(cat commit1) $(git rev-parse refs/heads/master) &&
	test $(git rev-parse refs/heads/master) = $(cat commit1)
'

test_expect_success 'update-ref-remote to unreferenced commit, passing oldval' '
	git update-ref-remote self refs/heads/master $(cat commit2) $(cat commit1) &&
	test $(git rev-parse refs/heads/master) = $(cat commit2)
'

# TODO: -d, --stdin, -z

test_done