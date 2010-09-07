#!/bin/sh

test_description='reset narrow repositories'

. ./test-lib.sh

test_expect_success setup '
	mkdir a &&
	touch a/foo a/bar foo bar &&
	git add a/foo a/bar foo bar &&
	git commit -m zero &&
	git tag C1 &&

	echo one >>a/foo &&
	echo one >>foo &&
	git add a/foo foo &&
	git commit -m one &&
	git tag C2 &&

	echo two >>a/foo &&
	echo two >>foo &&
	git add a/foo foo &&
	git commit -m two &&
	git tag C3 &&

	echo a >.git/narrow &&
	rm .git/index &&
	git update-index --narrow-base C1^{tree} &&
	git add a/foo &&
	rm foo bar
'

test_expect_success 'resetting HEAD^' '
	git reset --hard HEAD^ &&
	git rev-parse --narrow-base >result &&
	git rev-parse C2^{tree} >expected &&
	test_cmp expected result
'

test_done
