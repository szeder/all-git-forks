#!/bin/sh

test_description='test archive submodules of git-archive'
. ./test-lib.sh

# Basic sanity

test_expect_success 'setup - initial commit' '
	echo testfile >t &&
	git add t &&
	git commit -m "initial commit" &&
	git branch initial
'

test_expect_success 'archive accepts recurse-submodules option' '
	git archive --recurse-submodules HEAD >/dev/null
'

test_expect_success 'archive complains about recurse-submodules with remote' '
	test_must_fail git archive --recurse-submodules --remote git@github.com:git/git.git >/dev/null
'

# Make a dir and clean it up afterwards
make_dir() {
	mkdir "$1" &&
	test_when_finished "rm -rf '$1'"
}

# Check that the dir given in "$1" contains exactly the
# set of paths given as arguments.
check_dir() {
	dir=$1; shift
	{
		echo "$dir" &&
		for i in "$@"; do
			echo "$dir/$i"
		done
	} | sort >expect &&
	find "$dir" ! -name pax_global_header -print | sort >actual &&
	test_cmp expect actual
}

submodurl=$(pwd -P)/mymodule

test_expect_success 'setup - repository with submodule' '
	mkdir mymodule &&
	(
		cd mymodule &&
		git init &&
		echo a >a &&
		git add a &&
		git commit -m "submodule commit 1" &&
		git tag -a -m "rev-m1" rev-m1
	) &&
	mkdir myproject &&
	(
		cd myproject &&
		git init &&
		echo b >b &&
		git add b &&
		git commit -m "project commit 1" &&
		git tag -a -m "rev-p1" rev-p1 &&
		git submodule add "$submodurl" funcs &&
		git commit -m "add submodule funcs"
	)
'

test_expect_success 'tar archive preserves existing submodule behaviour' '
	make_dir extract &&
	(
		cd myproject &&
		git archive --format=tar HEAD >../submodule.tar 
	) &&
	"$TAR" xf submodule.tar -C extract &&
	check_dir extract .gitmodules b funcs
'

test_expect_success 'tar archive includes submodule' '
	make_dir extract &&
	(
		cd myproject &&
		git archive --recurse-submodules --format=tar HEAD >../submodule.tar 
	) &&
	"$TAR" xf submodule.tar -C extract &&
	check_dir extract .gitmodules b funcs funcs/a
	test_cmp extract/funcs/a myproject/funcs/a
'

test_done
