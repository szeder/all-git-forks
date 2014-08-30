#!/bin/sh

test_description='test git rev-parse references'

. ./test-lib.sh

test_expect_success 'make repo' '
	git commit --allow-empty -m commit1 &&
	git commit --allow-empty -m commit2 &&
	git checkout  --detach master &&
	git commit --allow-empty -m commit3
'

head_sha1=`git rev-parse HEAD`

test_expect_success 'HEAD should be listed in rev-parse --all' '
	git rev-parse --all >all_refs &&
	grep -q "$head_sha1" all_refs
'

test_done
