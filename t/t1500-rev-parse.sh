#!/bin/sh

test_description='test git rev-parse'
. ./test-lib.sh

test_rev_parse() {
	name=$1
	shift

	test_expect_success "$name: quiet" \
	"test_expect_code $1 git rev-parse -q --is-inside-git-dir --is-inside-work-tree --is-bare-repository"
	shift
	[ $# -eq 0 ] && return

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

# label return_code is-bare is-inside-git is-inside-work prefix git-dir

ROOT=$(pwd)

test_rev_parse toplevel 2 false false true '' .git

cd .git || exit 1
test_rev_parse .git/ 1 false true false '' .
cd objects || exit 1
test_rev_parse .git/objects/ 1 false true false '' "$ROOT/.git"
cd ../.. || exit 1

mkdir -p sub/dir || exit 1
cd sub/dir || exit 1
test_rev_parse subdirectory 2 false false true sub/dir/ "$ROOT/.git"
cd ../.. || exit 1

git config core.bare true
test_rev_parse 'core.bare = true' 4 true false false

git config --unset core.bare
test_rev_parse 'core.bare undefined' 2 false false true

mkdir work || exit 1
cd work || exit 1
GIT_DIR=../.git
GIT_CONFIG="$(pwd)"/../.git/config
export GIT_DIR GIT_CONFIG

git config core.bare false
test_rev_parse 'GIT_DIR=../.git, core.bare = false' 2 false false true ''

git config core.bare true
test_rev_parse 'GIT_DIR=../.git, core.bare = true' 4 true false false ''

git config --unset core.bare
test_rev_parse 'GIT_DIR=../.git, core.bare undefined' 2 false false true ''

mv ../.git ../repo.git || exit 1
GIT_DIR=../repo.git
GIT_CONFIG="$(pwd)"/../repo.git/config

git config core.bare false
test_rev_parse 'GIT_DIR=../repo.git, core.bare = false' 2 false false true ''

git config core.bare true
test_rev_parse 'GIT_DIR=../repo.git, core.bare = true' 4 true false false ''

git config --unset core.bare
test_rev_parse 'GIT_DIR=../repo.git, core.bare undefined' 2 false false true ''

test_done
