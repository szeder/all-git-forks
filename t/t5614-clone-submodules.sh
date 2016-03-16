#!/bin/sh

test_description='Test shallow cloning of repos with submodules'

. ./test-lib.sh

test_expect_success 'setup' '
	git checkout -b master &&
	test_commit commit1 &&
	test_commit commit2 &&
	mkdir sub &&
	(
		cd sub &&
		git init &&
		test_commit subcommit1 &&
		test_commit subcommit2
	) &&
	git submodule add ./sub &&
	git commit -m "add submodule"
'

test_expect_success 'nonshallow clone implies nonshallow submodule' '
	test_when_finished "rm -rf super_clone" &&
	git clone --recurse-submodules . super_clone &&
	(
		cd super_clone &&
		git log --oneline >lines &&
		test_line_count = 3 lines
	) &&
	(
		cd super_clone/sub &&
		git log --oneline >lines &&
		test_line_count = 2 lines
	)
'

test_expect_success 'shallow clone implies shallow submodule' '
	test_when_finished "rm -rf super_clone" &&
	git clone --recurse-submodules --no-local --depth 1 . super_clone &&
	(
		cd super_clone &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	) &&
	(
		cd super_clone/sub &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	)
'

test_expect_success 'shallow clone with non shallow submodule' '
	test_when_finished "rm -rf super_clone" &&
	git clone --recurse-submodules --no-local --depth 1 --no-shallow-submodules . super_clone &&
	(
		cd super_clone &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	) &&
	(
		cd super_clone/sub &&
		git log --oneline >lines &&
		test_line_count = 2 lines
	)
'

test_expect_success 'non shallow clone with shallow submodule' '
	test_when_finished "rm -rf super_clone" &&
	git clone --recurse-submodules --no-local --shallow-submodules . super_clone &&
	(
		cd super_clone &&
		git log --oneline >lines &&
		test_line_count = 3 lines
	) &&
	(
		cd super_clone/sub &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	)
'

test_done
