#!/bin/sh

test_description='test git rev-parse'
. ./test-lib.sh

test_rev_parse() {
	name=$1
	shift

	test_expect_success "$name: is-bare-repository" \
	"test '$1' = \"\$(git rev-parse --is-bare-repository)\""
	shift
	[ $# -eq 0 ] && return

	test_expect_success "$name: is-inside-git-dir" \
	"test '$1' = \"\$(git rev-parse --is-inside-git-dir)\""
	shift
	[ $# -eq 0 ] && return

	test_expect_success "$name: is-inside-work-tree" \
	"test '$1' = \"\$(git rev-parse --is-inside-work-tree)\""
	shift
	[ $# -eq 0 ] && return

	test_expect_success "$name: prefix" \
	"test '$1' = \"\$(git rev-parse --show-prefix)\""
	shift
	[ $# -eq 0 ] && return

	test_expect_success "$name: git-dir" \
	"test '$1' = \"\$(git rev-parse --git-dir)\""
	shift
	[ $# -eq 0 ] && return
}

# label is-bare is-inside-git is-inside-work prefix git-dir

ROOT=$(pwd)
original_core_bare=$(git config core.bare)

test_rev_parse toplevel false false true '' .git

cd .git || exit 1
test_rev_parse .git/ false true false '' .
cd objects || exit 1
test_rev_parse .git/objects/ false true false '' "$ROOT/.git"
cd ../.. || exit 1

mkdir -p sub/dir || exit 1
cd sub/dir || exit 1
test_rev_parse subdirectory false false true sub/dir/ "$ROOT/.git"
cd ../.. || exit 1

git config core.bare true
test_rev_parse 'core.bare = true' true false false

git config --unset core.bare
test_rev_parse 'core.bare undefined' false false true

mkdir work || exit 1
cd work || exit 1
GIT_DIR=../.git
GIT_CONFIG="$(pwd)"/../.git/config
export GIT_DIR GIT_CONFIG

git config core.bare false
test_rev_parse 'GIT_DIR=../.git, core.bare = false' false false true ''

git config core.bare true
test_rev_parse 'GIT_DIR=../.git, core.bare = true' true false false ''

git config --unset core.bare
test_rev_parse 'GIT_DIR=../.git, core.bare undefined' false false true ''

mv ../.git ../repo.git || exit 1
GIT_DIR=../repo.git
GIT_CONFIG="$(pwd)"/../repo.git/config

git config core.bare false
test_rev_parse 'GIT_DIR=../repo.git, core.bare = false' false false true ''

git config core.bare true
test_rev_parse 'GIT_DIR=../repo.git, core.bare = true' true false false ''

git config --unset core.bare
test_rev_parse 'GIT_DIR=../repo.git, core.bare undefined' false false true ''

test_expect_success 'cleanup from the previous tests' '
	cd .. &&
	rm -r work &&
	mv repo.git .git &&
	unset GIT_DIR &&
	unset GIT_CONFIG &&
	git config core.bare $original_core_bare
'

test_expect_success 'git-common-dir from worktree root' '
	echo .git >expect &&
	git rev-parse --git-common-dir >actual &&
	test_cmp expect actual
'

test_expect_success 'git-common-dir inside sub-dir' '
	mkdir -p path/to/child &&
	test_when_finished "rm -rf path" &&
	echo "$(git -C path/to/child rev-parse --show-cdup).git" >expect &&
	git -C path/to/child rev-parse --git-common-dir >actual &&
	test_cmp expect actual
'

test_expect_success 'git-path from worktree root' '
	echo .git/objects >expect &&
	git rev-parse --git-path objects >actual &&
	test_cmp expect actual
'

test_expect_success 'git-path inside sub-dir' '
	mkdir -p path/to/child &&
	test_when_finished "rm -rf path" &&
	echo "$(git -C path/to/child rev-parse --show-cdup).git/objects" >expect &&
	git -C path/to/child rev-parse --git-path objects >actual &&
	test_cmp expect actual
'

test_done
