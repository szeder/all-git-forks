#!/bin/sh

test_description='test push-merge feature'

. ./test-lib.sh

test_expect_success 'init upstream' '
	git config receive.denyCurrentBranch ignore &&
	test_commit init
'

test_expect_success 'setup clone' '
	git clone .git clone
	(
		cd clone &&
		test_commit commit1_client
	)
'

test_expect_success 'advance upstream' 'test_commit commit1'

init_hash=$(git rev-parse init)
commit1_hash=$(git rev-parse commit1)
commit1_client_hash=$( (cd clone && git rev-parse commit1_client) )

test_expect_success 'commit1 not accessible' '
	(
		cd clone &&
		test_must_fail git log "$commit1_hash"
	)
'

test_expect_success 'push-merge smoke: run' '
	git reset --hard commit1 &&
	git branch branch2 init &&
	git branch branch3 master &&
	(
		cd clone &&
		GIT_EDITOR="echo merge >\"\$1\"" &&
		export GIT_EDITOR &&
		test_config "push.refs/remotes/origin/master.merge" always &&
		test_config push.default upstream &&
		test_config branch.master.merge refs/heads/branch3 &&
		git push origin
	)
'

head_hash=$(git rev-parse HEAD)

test_expect_success 'push-merge smote: client not advanced' '
	(
		cd clone &&
		git rev-parse HEAD
	) >actual &&
	echo "$commit1_client_hash" >expect &&
	test_cmp expect actual
'

test_expect_success 'push-merge smoke: check' '
	echo "$head_hash $commit1_hash $commit1_client_hash" >expect &&
	echo "$commit1_client_hash $init_hash" >>expect &&
	echo "$commit1_hash $init_hash" >>expect &&
	echo "$init_hash " >>expect &&
	git log --date-order "--pretty=tformat:%H %P" >actual &&
	test_cmp expect actual
'

test_done
