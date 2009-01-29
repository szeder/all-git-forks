#!/bin/sh

test_description='git init'

. ./test-lib.sh

check_config () {
	if test -d "$1" && test -f "$1/config" && test -d "$1/refs"
	then
		: happy
	else
		echo "expected a directory $1, a file $1/config and $1/refs"
		return 1
	fi
	bare=$(GIT_CONFIG="$1/config" git config --bool core.bare)
	worktree=$(GIT_CONFIG="$1/config" git config core.worktree) ||
	worktree=unset

	test "$bare" = "$2" && test "$worktree" = "$3" || {
		echo "expected bare=$2 worktree=$3"
		echo "     got bare=$bare worktree=$worktree"
		return 1
	}
}

test_expect_success 'plain' '
	(
		unset GIT_DIR GIT_WORK_TREE
		mkdir plain &&
		cd plain &&
		git init
	) &&
	check_config plain/.git false unset
'

test_expect_success 'plain with GIT_WORK_TREE' '
	if (
		unset GIT_DIR
		mkdir plain-wt &&
		cd plain-wt &&
		GIT_WORK_TREE=$(pwd) git init
	)
	then
		echo Should have failed -- GIT_WORK_TREE should not be used
		false
	fi
'

test_expect_success 'plain bare' '
	(
		unset GIT_DIR GIT_WORK_TREE GIT_CONFIG
		mkdir plain-bare-1 &&
		cd plain-bare-1 &&
		git --bare init
	) &&
	check_config plain-bare-1 true unset
'

test_expect_success 'plain bare with GIT_WORK_TREE' '
	if (
		unset GIT_DIR GIT_CONFIG
		mkdir plain-bare-2 &&
		cd plain-bare-2 &&
		GIT_WORK_TREE=$(pwd) git --bare init
	)
	then
		echo Should have failed -- GIT_WORK_TREE should not be used
		false
	fi
'

test_expect_success 'GIT_DIR bare' '

	(
		unset GIT_CONFIG
		mkdir git-dir-bare.git &&
		GIT_DIR=git-dir-bare.git git init
	) &&
	check_config git-dir-bare.git true unset
'

test_expect_success 'init --bare' '

	(
		unset GIT_DIR GIT_WORK_TREE GIT_CONFIG
		mkdir git-init-bare.git &&
		cd git-init-bare.git &&
		git init --bare
	) &&
	check_config git-init-bare.git true unset
'

test_expect_success 'GIT_DIR non-bare' '

	(
		unset GIT_CONFIG
		mkdir non-bare &&
		cd non-bare &&
		GIT_DIR=.git git init
	) &&
	check_config non-bare/.git false unset
'

test_expect_success 'GIT_DIR & GIT_WORK_TREE (1)' '

	(
		unset GIT_CONFIG
		mkdir git-dir-wt-1.git &&
		GIT_WORK_TREE=$(pwd) GIT_DIR=git-dir-wt-1.git git init
	) &&
	check_config git-dir-wt-1.git false "$(pwd)"
'

test_expect_success 'GIT_DIR & GIT_WORK_TREE (2)' '

	if (
		unset GIT_CONFIG
		mkdir git-dir-wt-2.git &&
		GIT_WORK_TREE=$(pwd) GIT_DIR=git-dir-wt-2.git git --bare init
	)
	then
		echo Should have failed -- --bare should not be used
		false
	fi
'

test_expect_success 'reinit' '

	(
		unset GIT_CONFIG GIT_WORK_TREE GIT_CONFIG

		mkdir again &&
		cd again &&
		git init >out1 2>err1 &&
		git init >out2 2>err2
	) &&
	grep "Initialized empty" again/out1 &&
	grep "Reinitialized existing" again/out2 &&
	>again/empty &&
	test_cmp again/empty again/err1 &&
	test_cmp again/empty again/err2
'

test_expect_success 'init with --template' '
	mkdir template-source &&
	echo content >template-source/file &&
	(
		mkdir template-custom &&
		cd template-custom &&
		git init --template=../template-source
	) &&
	test_cmp template-source/file template-custom/.git/file
'

test_expect_success 'init with --template (blank)' '
	(
		mkdir template-plain &&
		cd template-plain &&
		git init
	) &&
	test -f template-plain/.git/info/exclude &&
	(
		mkdir template-blank &&
		cd template-blank &&
		git init --template=
	) &&
	! test -f template-blank/.git/info/exclude
'

test_done
