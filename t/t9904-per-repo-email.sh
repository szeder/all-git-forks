#!/bin/sh
#
# Copyright (c) 2016 Dan Aloni
#

test_description='per-repo forced setting of email address'

. ./test-lib.sh

prepare () {
	# Have a non-empty repository
	rm -fr .git
	git init
	echo "Initial" >foo &&
	git add foo &&
	EDITOR=: VISUAL=: git commit -m foo &&

	# Setup a likely user.useConfigOnly use case
	unset GIT_AUTHOR_NAME &&
	unset GIT_AUTHOR_EMAIL &&
	test_unconfig --global user.name &&
	test_unconfig --global user.email &&
	test_config user.name "test" &&
	test_unconfig user.email &&
	test_config_global user.useConfigOnly true
}

about_to_commit () {
	echo "Second" >>foo &&
	git add foo
}

test_expect_success 'fails committing if clone email is not set' '
	prepare && about_to_commit &&

	EDITOR=: VISUAL=: test_must_fail git commit -m msg
'

test_expect_success 'fails committing if clone email is not set, but EMAIL set' '
	prepare && about_to_commit &&

	EMAIL=test@fail.com EDITOR=: VISUAL=: test_must_fail git commit -m msg
'

test_expect_success 'succeeds committing if clone email is set' '
	prepare && about_to_commit &&

	test_config user.email "test@ok.com" &&
	EDITOR=: VISUAL=: git commit -m msg
'

test_expect_success 'succeeds cloning if global email is not set' '
	prepare &&

	git clone . clone
'

test_done
