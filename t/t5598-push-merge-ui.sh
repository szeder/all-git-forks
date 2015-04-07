#!/bin/sh

test_description='test push-merge feature UI experiments'

. ./test-lib.sh

test_expect_success 'setup upstream' '
	git config receive.denyCurrentBranch ignore &&
	test_commit init
'

test_expect_success 'setup clone' '
	git clone --single-branch .git clone &&
	(
		cd clone &&
		test_commit commit1_client
	)
'

test_expect_success 'advance upstream' '
	test_commit commit1
'

commit1_hash=$(git rev-parse commit1)

test_expect_failure 'push-merge smoke: see ref in push-hook' '
	test_when_finished "rm -v clone/.git/hooks/pre-push" &&
	(
		cd clone &&
                cat >.git/hooks/pre-push <<-"EOF" &&
#!/bin/sh

set -e

git log "$commit1_hash"
EOF
                chmod +x .git/hooks/pre-push &&
                export commit1_hash &&
                git push origin master:master
	)
'

test_expect_success 'push forced' '
	(
		cd clone &&
		git push origin +master:master &&
		git branch -a -v
	)
'

test_done
