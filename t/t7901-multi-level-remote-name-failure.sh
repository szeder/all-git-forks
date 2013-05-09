#!/bin/sh

test_description='Show why multi-level remote names are a bad idea'

. ./test-lib.sh

test_expect_success 'setup old-style multi-level remote' '
	test_commit a &&
	git config remote.multi/old.url . &&
	git config remote.multi/old.fetch "+refs/heads/*:refs/remotes/multi/old/*" &&
	git fetch multi/old
'

test_expect_success 'Succeed in using shorthand notation: "$remote/$branch"' '
	git rev-parse --verify a >expect &&
	git rev-parse --verify multi/old/master >actual &&
	test_cmp expect actual
'

test_expect_success 'setup new-style multi-level remote' '
	git config remote.multi/new.url . &&
	git config remote.multi/new.fetch "+refs/heads/*:refs/peers/multi/new/heads/*" &&
	git fetch multi/new
'

test_expect_failure 'Fail to use shorthand notation: "$remote/$branch"' '
	git rev-parse --verify a >expect &&
	git rev-parse --verify multi/new/master >actual &&
	test_cmp expect actual
'

test_done
