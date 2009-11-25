#!/bin/sh
#
# Copyright (c) 2009 Sam Vilain
#

test_description='mirror fetch test'

. ./test-lib.sh

test_expect_success setup '
	echo >file master initial &&
	git add file &&
	git commit -a -m "Master initial" &&
	git clone . master &&
	git clone master mirror &&
	cd master &&
	echo >file master update &&
	git commit -a -m "Master update" &&
	cd .. &&
	mkdir clone &&
	cd clone &&
	git init &&
	git remote add origin ../master &&
	git config remote.origin.mirror-url ../mirror
'

# in later iterations we'll expect these mirror tracking refs to be
# cleaned up once they are confirmed reachable from the master, but
# for now they leave a sufficient breadcrumb of the operation

test_expect_success 'fetch using mirror - explicit' '
	git fetch --use-mirror origin refs/heads/*:refs/remotes/origin/* &&
	git rev-parse refs/mirrors/origin/localhost/heads/master
'

test_expect_success 'fetch using mirror - default' '
	cd .. &&
	mkdir clone2 &&
	cd clone2 &&
	git init &&
	git remote add origin ../master &&
	git config remote.origin.mirror-url ../mirror
	git fetch --use-mirror &&
	git rev-parse refs/mirrors/origin/localhost/heads/master
'
test_done
