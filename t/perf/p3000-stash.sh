#!/bin/sh

test_description="Test performance of git stash"

. ./perf-lib.sh

test_perf_default_repo

file=$(git ls-files | tail -n 30 | head -n 1)

test_expect_success "prepare repository" "
	echo x >$file
"

test_perf "stash/stash pop" "
	git stash &&
	git stash pop
"

test_done
