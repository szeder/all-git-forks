#!/bin/sh

test_description='test git rev-parse'
. ./test-lib.sh

# usage: [options] label is-bare is-inside-git is-inside-work prefix git-dir
test_rev_parse () {
	dir=
	bare=
	env=
	while :
	do
		case "$1" in
		-C) dir="-C $2"; shift; shift ;;
		-b) bare="$2"; shift; shift ;;
		-g) env="GIT_DIR=$2; export GIT_DIR"; shift; shift ;;
		-*) error "test_rev_parse: unrecognized option '$1'" ;;
		*) break ;;
		esac
	done

	case "$bare" in
	'') ;;
	t*) bare="test_config $dir core.bare true" ;;
	f*) bare="test_config $dir core.bare false" ;;
	u*) bare="test_unconfig $dir core.bare" ;;
	*) error "test_rev_parse: unrecognized core.bare value '$bare'" ;;
	esac

	name=$1
	shift

	for o in is-bare-repository \
		 is-inside-git-dir \
		 is-inside-work-tree \
		 show-prefix \
		 git-dir
	do
		expect="$1"
		test_expect_success "$name: $o" '
			test_when_finished "sane_unset GIT_DIR" &&
			eval $env &&
			$bare &&
			echo "$expect" >expect &&
			git $dir rev-parse --$o >actual &&
			test_cmp expect actual
		'
		shift
		test $# -eq 0 && break
	done
}

ROOT=$(pwd)

test_rev_parse toplevel false false true '' .git

test_rev_parse -C .git .git/ false true false '' .
test_rev_parse -C .git/objects .git/objects/ false true false '' "$ROOT/.git"

test_expect_success 'setup untracked sub/dir' 'mkdir -p sub/dir'
test_rev_parse -C sub/dir subdirectory false false true sub/dir/ "$ROOT/.git"

test_rev_parse -b t 'core.bare = true' true false false

test_rev_parse -b u 'core.bare undefined' false false true

test_expect_success 'setup non-local database ../.git' 'mkdir work'

test_rev_parse -C work -g ../.git -b f 'GIT_DIR=../.git, core.bare = false' false false true ''

test_rev_parse -C work -g ../.git -b t 'GIT_DIR=../.git, core.bare = true' true false false ''

test_rev_parse -C work -g ../.git -b u 'GIT_DIR=../.git, core.bare undefined' false false true ''

test_expect_success 'setup non-local database ../repo.git' 'cp -R .git repo.git'

test_rev_parse -C work -g ../repo.git -b f 'GIT_DIR=../repo.git, core.bare = false' false false true ''

test_rev_parse -C work -g ../repo.git -b t 'GIT_DIR=../repo.git, core.bare = true' true false false ''

test_rev_parse -C work -g ../repo.git -b u 'GIT_DIR=../repo.git, core.bare undefined' false false true ''

test_done
