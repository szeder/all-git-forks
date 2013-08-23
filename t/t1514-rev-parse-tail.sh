#!/bin/sh

test_description='test <branch>@{upstream} syntax'

. ./test-lib.sh

test_expect_success 'setup' '
	echo one > content &&
	git add content &&
	git commit -m one &&
	git checkout -b test master &&
	echo two > new &&
	git add new &&
	git commit -a -m two
'

test_expect_success 'test tail creation' '
	git rev-parse refs/tails/test > actual &&
	git rev-parse master > expect &&
	test_cmp expect actual
'

test_expect_success 'test @{tail}' '
	git rev-parse test@{tail} > actual &&
	git rev-parse master > expect &&
	test_cmp expect actual
'

test_expect_success 'test rebase tail update' '
	git checkout -b next master &&
	echo three > content &&
	git commit -a -m three &&
	git rebase --onto next test@{tail} test &&
	git rev-parse test@{tail} > actual &&
	git rev-parse next > expect &&
	test_cmp expect actual
'

test_done
