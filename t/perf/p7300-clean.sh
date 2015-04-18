#!/bin/sh

test_description="Test git-clean performance"

. ./perf-lib.sh

test_perf_large_repo
test_checkout_worktree

test_expect_success 'setup untracked directory with many sub dirs' '
	rm -rf 500_sub_dirs 50000_sub_dirs clean_test_dir &&
	mkdir 500_sub_dirs 50000_sub_dirs clean_test_dir &&
	for i in $(test_seq 1 500)
	do
		mkdir 500_sub_dirs/dir$i || return $?
	done &&
	for i in $(test_seq 1 100)
	do
		cp -r 500_sub_dirs 50000_sub_dirs/dir$i || return $?
	done
'

test_perf 'clean many untracked sub dirs, check for nested git' '
	rm -rf clean_test_dir/50000_sub_dirs_cpy &&
	cp -r 50000_sub_dirs clean_test_dir/50000_sub_dirs_cpy &&
	git clean -q -f -d  clean_test_dir/ &&
	test_dir_is_empty clean_test_dir
'

test_perf 'clean many untracked sub dirs, ignore nested git' '
	rm -rf clean_test_dir/50000_sub_dirs_cpy &&
	cp -r 50000_sub_dirs clean_test_dir/50000_sub_dirs_cpy &&
	git clean -q -f -f -d  clean_test_dir/ &&
	test_dir_is_empty clean_test_dir
'

test_done
