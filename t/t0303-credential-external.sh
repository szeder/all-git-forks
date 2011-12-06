#!/bin/sh

test_description='external credential helper tests'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-credential.sh

if test -z "$GIT_TEST_CREDENTIAL_HELPER"; then
	say "# skipping external helper tests (set GIT_TEST_CREDENTIAL_HELPER)"
else
	# clean before the test in case there is cruft left
	# over from a previous run that would impact results
	helper_test_clean "$GIT_TEST_CREDENTIAL_HELPER"

	helper_test "$GIT_TEST_CREDENTIAL_HELPER"

	# then clean afterwards so that we are good citizens
	# and don't leave cruft in the helper's storage, which
	# might be long-term system storage
	helper_test_clean "$GIT_TEST_CREDENTIAL_HELPER"
fi

if test -z "$GIT_TEST_CREDENTIAL_HELPER_TIMEOUT"; then
	say "# skipping external helper timeout tests"
else
	helper_test_clean "$GIT_TEST_CREDENTIAL_HELPER_TIMEOUT"
	helper_test_timeout "$GIT_TEST_CREDENTIAL_HELPER_TIMEOUT"
	helper_test_clean "$GIT_TEST_CREDENTIAL_HELPER_TIMEOUT"
fi

test_done
