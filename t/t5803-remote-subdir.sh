#!/bin/sh

test_description='subdir:: remote helper'
. ./test-lib.sh

setup_subdir_remote() {
	git remote add "$1" "$2" &&
	git config "remote.${1}.vcs" subdir &&
	git config "remote.${1}.dirspec" "$3"
}

test_expect_success 'setup repository' '
	git init server &&
	(cd server &&
	 echo spam >spam &&
	 mkdir eggs &&
	 echo eggs/fried >eggs/fried &&
	 echo eggs/scrambled >eggs/scrambled &&
	 mkdir bacon &&
	 echo bacon/fried >bacon/fried &&
	 git add . &&
	 git commit -m one) &&
	setup_subdir_remote "noop" "${PWD}/server" ":"
	setup_subdir_remote "extract_bacon" "${PWD}/server" "bacon:"
'

test_expect_success 'ls-remote against noop remote' '
	expect_sha1=$(git --git-dir=server/.git rev-parse --verify master) &&
	echo "$expect_sha1	refs/heads/master" >>expect &&
	git ls-remote noop >actual &&
	test_cmp expect actual
'

test_expect_success 'ls-remote against misconfigured remote fails' '
	setup_subdir_remote "misconfigured" "${PWD}/server" "missing_colon" &&
	test_must_fail git ls-remote misconfigured
'

# Cannot ls-remote against bare URL

test_expect_success 'fetch from noop remote does not rewrite commit' '
	(cd server &&
	 git rev-parse --verify master) >expect &&
	git fetch noop &&
	git rev-parse --verify refs/remotes/noop/master >actual &&
	test_cmp expect actual
'

test_expect_success 'fetch from extracting remote extracts subdir -> topdir' '
	(cd server &&
	 git rev-parse --verify master:bacon) >expect_tree &&
	git fetch extract_bacon &&
	git rev-parse --verify refs/remotes/extract_bacon/master: >actual_tree &&
	test_cmp expect_tree actual_tree
'

# fetch from inserting remote
# fetch from combination remote
# fetch multiple refs
# updating fetch leverages existing history
# tag handling?

test_done
