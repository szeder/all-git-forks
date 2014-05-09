#!/bin/sh

test_description='update'

. ./test-lib.sh

check () {
	git rev-parse -q --verify $1 > expected &&
	git rev-parse -q --verify $2 > actual &&
	test_cmp expected actual
}

check_msg () {
	if test "$1" = "$2"
	then
		echo "Update branch '$1'" > expected
	else
		test "$2" != "master" && into=" into $2"
		echo "Merge branch '$1'${into}" > expected
	fi &&
	git log -1 --format=%s > actual &&
	test_cmp expected actual
}

test_expect_success 'setup' '
	echo one > file &&
	git add file &&
	git commit -a -m one &&
	echo two > file &&
	git commit -a -m two &&
	git clone . remote &&
	git remote add origin remote
'

test_expect_success 'basic update' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git reset --hard @^ &&
	git update &&
	check master origin/master
	)
'

test_expect_success 'basic update without upstream' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git reset --hard @^ &&
	git branch --unset-upstream &&
	git update &&
	check master origin/master
	)
'

test_expect_success 'basic update no-ff' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git reset --hard @^ &&
	git update --no-ff &&
	check @^1 origin/master &&
	check_msg master master
	)
'

test_expect_success 'git update non-fast-forward' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git checkout -b other master^ &&
	>new &&
	git add new &&
	git commit -m new &&
	git checkout -b test -t other &&
	git reset --hard master &&
	test_must_fail git update &&
	check @ master
	)
'

test_expect_success 'git update non-fast-forward with merge' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git checkout -b other master^ &&
	>new &&
	git add new &&
	git commit -m new &&
	git checkout -b test -t other &&
	git reset --hard master &&
	git update --merge &&
	check @^2 master &&
	check @^1 other &&
	check_msg test other
	)
'

test_expect_success 'git update non-fast-forward with rebase' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git checkout -b other master^ &&
	>new &&
	git add new &&
	git commit -m new &&
	git checkout -b test -t other &&
	git reset --hard master &&
	git update --rebase &&
	check @^ other
	)
'

test_expect_success 'git update with argument' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git checkout -b test &&
	git reset --hard @^ &&
	git update master &&
	check test master
	)
'

test_expect_success 'git update with remote argument' '
	test_when_finished "rm -rf test" &&
	(
	git clone . test &&
	cd test &&
	git checkout -b test &&
	git reset --hard @^ &&
	git update origin/master &&
	check test master
	)
'

test_done
