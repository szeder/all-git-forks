#!/bin/sh

test_description='pull with --set-upstream'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-terminal.sh

ensure_fresh_upstream() {
	rm -rf parent &&
	git init parent &&
	cd parent &&
	(
		echo content >file &&
		git add file &&
		git commit -m one &&
		git checkout -b other &&
		echo content >file2 &&
		git add file2 &&
		git commit -m two &&
		git checkout -b master2 &&
		git checkout master
	) &&
	cd ..
}

test_expect_success 'setup bare parent' '
	ensure_fresh_upstream &&
	git remote add upstream parent &&
	git pull upstream master
'

check_config() {
	(echo $2; echo $3) >expect.$1
	(git config branch.$1.remote
	 git config branch.$1.merge) >actual.$1
	test_cmp expect.$1 actual.$1
}

test_expect_success 'pull -u master' '
	git pull -u upstream master &&
	check_config master upstream refs/heads/master
'

test_expect_success 'pull -u master:other' '
	git pull -u upstream master:other &&
	check_config other upstream refs/heads/master
'

test_expect_success 'pull -u --dry-run other:other' '
	git pull -u --dry-run upstream other:other &&
	check_config other upstream refs/heads/master
'

test_expect_success 'pull -u master2:master2 master:other' '
	git branch master2 &&
	git pull -u upstream master2:master2 master:other &&
	check_config master2 upstream refs/heads/master2 &&
	check_config other upstream refs/heads/master
'

test_expect_success 'pull -u HEAD' '
	git clone parent son &&
	cd son &&
	git checkout -b headbranch &&
	git pull -u origin HEAD &&
	check_config headbranch origin refs/heads/master
'

test_expect_success TTY 'quiet pull' '
	ensure_fresh_upstream &&

	test_terminal git pull -u --quiet upstream master 2>&1 | tee output &&
	test_cmp /dev/null output
'

test_done
