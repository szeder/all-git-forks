#!/bin/sh
#
# Copyright (c) 2015 Stefan Beller
#

test_description='Testing version 2 of the fetch protocol'

. ./test-lib.sh

mk_repo_pair () {
	rm -rf client server &&
	test_create_repo client &&
	test_create_repo server &&
	(
		cd server &&
		git config receive.denyCurrentBranch warn
	) &&
	(
		cd client &&
		git remote add origin ../server
		git config remote.origin.transportversion 2
	)
}

test_expect_success 'setup' '
	mk_repo_pair &&
	(
		cd server &&
		test_commit one
	) &&
	(
		cd client &&
		git fetch origin master
	)
'

# More to come here, similar to t5515 having a sub directory full of expected
# data going over the wire.

test_done
