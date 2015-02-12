#!/bin/sh

test_description='git commit races'
. ./test-lib.sh

test_tick

test_expect_success 'set up editor' '
	write_script editor <<-\EOF
	git commit --allow-empty -m hare
	echo tortoise >"$1"
	EOF
'

test_expect_success 'race to create orphan commit' '
	test_must_fail env EDITOR=./editor git commit --allow-empty &&
	git show -s --pretty=format:%s >subject &&
	grep -q hare subject &&
	test -z "$(git show -s --pretty=format:%P)"
'

test_expect_success 'race to create non-orphan commit' '
	git checkout --orphan branch &&
	git commit --allow-empty -m base &&
	git rev-parse HEAD >base &&
	test_must_fail env EDITOR=./editor git commit --allow-empty &&
	git show -s --pretty=format:%s >subject &&
	grep -q hare subject &&
	git rev-parse HEAD^ >parent &&
	test_cmp base parent
'

test_done
