#!/bin/sh

test_description='signed tag tests'
. ./test-lib.sh
. "$TEST_DIRECTORY/lib-gpg.sh"

test_expect_success GPG 'create signed tags' '
	echo 1 >file && git add file &&
	test_tick && git commit -m initial &&
	git tag -s -m initial initial &&
	git branch side &&
	echo a &&

	echo 2 >file && test_tick && git commit -a -m second &&
	git tag -s -m second second &&
	echo b &&

	git checkout side &&
	echo 3 >elif && git add elif &&
	test_tick && git commit -m "third on side" &&
	echo c &&

	git checkout master &&
	test_tick && git merge -S side &&
	git tag -s -m merge merge &&
	echo d &&

	echo 4 >file && test_tick && git commit -a -S -m "fourth unsigned" &&
	git tag -a -m fourth-unsigned fourth-unsigned &&
	echo e &&

	test_tick && git commit --amend -S -m "fourth signed" &&
	git tag -s -m fourth fourth-signed &&
	echo f &&

	echo 5 >file && test_tick && git commit -a -m "fifth" &&
	git tag fifth-unsigned &&
	echo g &&

	git config commit.gpgsign true &&
	echo 6 >file && test_tick && git commit -a -m "sixth" &&
	git tag -a -m sixth sixth-unsigned &&
	echo h &&

	test_tick && git rebase -f HEAD^^ && git tag -s -m 6th sixth-signed HEAD^ &&
	git tag -m seventh -s seventh-signed &&
	echo i &&

	echo 8 >file && test_tick && git commit -a -m eighth &&
	git tag -uB7227189 -m eighth eighth-signed-alt &&
	echo j
'

test_expect_success GPG 'verify and show signatures' '
	(
		for tag in initial second merge fourth-signed sixth-signed seventh-signed
		do
			git verify-tag $tag 2>actual &&
			grep "Good signature from" actual &&
			! grep "BAD signature from" actual &&
			echo $tag OK || exit 1
		done
	) &&
	(
		for tag in fourth-unsigned fifth-unsigned sixth-unsigned
		do
			test_must_fail git verify-tag $tag 2>actual &&
			! grep "Good signature from" actual &&
			! grep "BAD signature from" actual &&
			echo $tag OK || exit 1
		done
	) &&
	(
		for tag in eighth-signed-alt
		do
			git verify-tag $tag 2>actual &&
			grep "Good signature from" actual &&
			! grep "BAD signature from" actual &&
			grep "not certified" actual &&
			echo $tag OK || exit 1
		done
	)
'

test_done
