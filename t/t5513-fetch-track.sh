#!/bin/sh

test_description='fetch follows remote-tracking branches correctly'

. ./test-lib.sh

test_expect_success setup '
	>file &&
	git add . &&
	test_tick &&
	git commit -m Initial &&
	git branch b-0 &&
	git branch b1 &&
	git branch b/one &&
	test_create_repo other &&
	(
		cd other &&
		git config remote.origin.url .. &&
		git config remote.origin.fetch "+refs/heads/b/*:refs/remotes/b/*"
	)
'

test_expect_success fetch '
	(
		cd other && git fetch origin &&
		test "$(git for-each-ref --format="%(refname)")" = refs/remotes/b/one
	)
'

cat >expected <<EOF
236e830928a4295f5473416501dd777933bb778e		branch 'master' of .
EOF

test_expect_success 'fetch default' '
	test_when_finished "rm -rf another" &&

	(
		test_create_repo another &&
		cd another &&
		git remote add origin .. &&
		echo test > file &&
		git add . &&
		git commit -m test &&
		git checkout -t -b local-tracking master &&
		git fetch &&
		test_cmp ../expected .git/FETCH_HEAD
	)
'
cat >expected <<EOF
9d34b142e42f6b3dbab46dd4b9bc515e0ab16101	not-for-merge	branch 'b-0' of ..
9d34b142e42f6b3dbab46dd4b9bc515e0ab16101	not-for-merge	branch 'b/one' of ..
9d34b142e42f6b3dbab46dd4b9bc515e0ab16101	not-for-merge	branch 'b1' of ..
9d34b142e42f6b3dbab46dd4b9bc515e0ab16101	not-for-merge	branch 'master' of ..
EOF

test_expect_success 'fetch default simple' '
	test_when_finished "rm -rf another" &&

	(
		test_create_repo another &&
		cd another &&
		git config fetch.default simple &&
		git remote add origin .. &&
		echo test > file &&
		git add . &&
		git commit -m test &&
		git checkout -t -b local-tracking master &&
		git fetch &&
		test_cmp ../expected .git/FETCH_HEAD
	)
'

test_done
