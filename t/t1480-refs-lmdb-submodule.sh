#!/bin/sh
#
# Copyright (c) 2016 Twitter, Inc
# Based on t5531-deep-submodule-push.sh

test_description='Test lmdb refs backend'
TEST_NO_CREATE_REPO=1
. ./test-lib.sh

if ! test_have_prereq LMDB
then
	skip_all="Skipping lmdb refs backend tests, lmdb backend not built"
	test_done
fi

test_expect_success setup '
	mkdir pub.git &&
	GIT_DIR=pub.git git init --bare &&
	GIT_DIR=pub.git git config receive.fsckobjects true &&
	mkdir work &&
	(
		cd work &&
		git init --ref-storage=lmdb &&
		git config push.default matching &&
		mkdir -p gar/bage &&
		(
			cd gar/bage &&
			git init --ref-storage=lmdb  &&
			git config push.default matching &&
			>junk &&
			git add junk &&
			git commit -m "Initial junk"
		) &&
		git add gar/bage &&
		git commit -m "Initial superproject"
	)
'

test_expect_success 'submodules have same ref storage' '
	git init --ref-storage=lmdb test &&
	(
		cd test &&
		git submodule add ../work/gar/bage w
	) &&
	(
		cd test/w &&
		git config extensions.refstorage >cfg &&
		echo lmdb >expect &&
		test_cmp cfg expect
	)
'

test_expect_success 'push with correct backend' '
	(
		cd work/gar/bage &&
		>junk2 &&
		git add junk2 &&
		git commit -m "Second junk"
	) &&
	(
		cd work &&
		git add gar/bage &&
		git commit -m "Second commit for gar/bage" &&
		git push --recurse-submodules=check ../pub.git master
	)
'

test_expect_success 'commit with different backend fails' '
	(
		cd work/gar/bage &&
		test_commit junk3 &&
		# manually convert to files-backend
		gitdir="$(git rev-parse --git-dir)" &&
		mkdir -p "$gitdir/refs/heads" &&
		git rev-parse HEAD >"$gitdir/refs/heads/master" &&
		git config --local --unset extensions.refStorage &&
		rm -r "$gitdir/refs.lmdb"
	) &&
	(
		cd work &&
		test_must_fail git add gar/bage
	)
'

test_done
