#!/bin/sh

# SKIPPED

test_description='test ff-refs'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit init &&
	for i in $(test_seq 1 9)
	do
		echo "data" >file_$i &&
		git add file_$i &&
		git commit -m"Commit $i" &&
		git branch br_$i
	done
'

test_expect_success 'UP-TO-DATE for equal branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git ff-refs >actual &&
		grep "UP-TO-DATE" actual
	)
'

test_expect_success 'UP-TO-DATE for ahead local branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git ff-refs >actual &&
		echo "data" >file_new &&
		git add file_new &&
		git commit -m"Commit new" &&
		grep "UP-TO-DATE" actual
	)
'

test_expect_success 'REMOTE-MISSING by local config change' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git config --replace branch.master.merge refs/heads/nothing &&
		git ff-refs >actual &&
		grep "REMOTE-MISSING" actual
	)
'

test_expect_success 'NON-FAST-FORWARD for diverged branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		echo "data" >file_new &&
		git add file_new &&
		git commit -m"Commit new" &&
		git ff-refs >actual &&
		grep "NON-FAST-FORWARD" actual
	)
'

test_expect_success 'UPDATED for fast-forwardable branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git ff-refs >actual &&
		grep "UPDATED" actual
	)
'

test_expect_success 'WOULD-UPDATE for dry-run on fast-forwardable branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git ff-refs --dry-run >actual &&
		grep "WOULD-UPDATE" actual
	)
'

test_expect_success 'SKIPPED for skip-worktrees on fast-forwardable branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git ff-refs --skip-worktrees >actual &&
		grep "SKIPPED" actual
	)
'

test_expect_success 'WOULD-SKIP for dry-run, skip-worktrees on fast-forwardable branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git ff-refs --dry-run --skip-worktrees >actual &&
		grep "WOULD-SKIP" actual
	)
'

test_expect_success 'UPDATE for fast-forwardable, not checked-out branch' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git checkout -b other origin/br_3 &&
		git ff-refs >actual &&
		grep "master" actual | grep "UPDATED"
	)
'

test_expect_success 'UPDATE for fast-forwardable, not checked-out branch using --skip-worktrees' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git checkout -b other origin/br_3 &&
		git ff-refs --skip-worktrees >actual &&
		grep "master" actual | grep "UPDATED"
	)
'

test_expect_success 'UPDATE multiple' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git checkout -b other origin/br_5 &&
		git reset --hard origin/br_3 &&
		git ff-refs >actual &&
		grep "master" actual | grep "UPDATED" &&
		grep "other" actual | grep "UPDATED"
	)
'

test_expect_success 'UPDATE one, skip worktree on another' '
	test_when_finished "rm -rf local" &&
	git clone . local &&
	(
		cd local &&
		git reset --hard origin/br_3 &&
		git checkout -b other origin/br_5 &&
		git reset --hard origin/br_3 &&
		git ff-refs --skip-worktrees >actual &&
		grep "master" actual | grep "UPDATED" &&
		grep "other" actual | grep "SKIPPED"
	)
'

test_done
