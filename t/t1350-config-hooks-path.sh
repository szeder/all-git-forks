#!/bin/sh

test_description='Test the core.hooksPath configuration variable'

. ./test-lib.sh

test_expect_success 'set up a pre-commit hook in core.hooksPath' '
	mkdir -p .git/custom-hooks .git/hooks &&
	write_script .git/custom-hooks/pre-commit <<-\EOF &&
printf "%s" "CUST" >>.git/PRE-COMMIT-HOOK-WAS-CALLED
EOF
	write_script .git/hooks/pre-commit <<-\EOF
printf "%s" "NORM" >>.git/PRE-COMMIT-HOOK-WAS-CALLED
EOF
'

test_expect_success 'Check that various forms of specifying core.hooksPath work' '
	test_commit no_custom_hook &&
	git config core.hooksPath .git/custom-hooks &&
	test_commit have_custom_hook &&
	git config core.hooksPath .git/custom-hooks/ &&
	test_commit have_custom_hook_trailing_slash &&
	git config core.hooksPath "$PWD/.git/custom-hooks" &&
	test_commit have_custom_hook_abs_path &&
	git config core.hooksPath "$PWD/.git/custom-hooks/" &&
	test_commit have_custom_hook_abs_path_trailing_slash &&
	printf "%s" "NORMCUSTCUSTCUSTCUST" >.git/PRE-COMMIT-HOOK-WAS-CALLED.expect &&
	test_cmp .git/PRE-COMMIT-HOOK-WAS-CALLED.expect .git/PRE-COMMIT-HOOK-WAS-CALLED
'

test_done
