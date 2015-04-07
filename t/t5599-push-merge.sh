#!/bin/sh

test_description='test push-merge feature'

. ./test-lib.sh

test_expect_success 'setup upstream' '
	git config receive.denyCurrentBranch ignore &&
	test_commit init &&
	test_commit commit1
'

test_expect_success 'setup clone' '
	git reset --hard init &&
	git clone .git clone
	(
		cd clone &&
		test_commit commit1_client
	)
'

test_expect_success 'push-merge smoke: run' '
	git reset --hard commit1 &&
	(
		cd clone &&
		git reset --hard commit1_client &&
		GIT_EDITOR="echo merge >\"\$1\"" &&
		export GIT_EDITOR &&
		git push-merge diverged origin refs/heads/master refs/remotes/origin/master 3>head_hash &&
		hash=$(cat head_hash)
		echo git push origin "$hash":master &&
		git push origin "$hash":master
	)
'

init_hash=$(git rev-parse init)
commit1_hash=$(git rev-parse commit1)
commit1_client_hash=$( (cd clone && git rev-parse commit1_client) )
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
