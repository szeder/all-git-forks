#!/bin/sh
#
# Copyright (c) 2010 Jakub Narebski
#

test_description='gitweb cache

This test checks GitwebCache::CacheOutput Perl module that is
responsible for capturing and caching gitweb output.'

# for now we are running only cache interface tests
. ./test-lib.sh

# this test is present in gitweb-lib.sh
if ! test_have_prereq PERL; then
	skip_all='perl not available, skipping test'
	test_done
fi

"$PERL_PATH" -MTest::More -e 0 >/dev/null 2>&1 || {
	skip_all='perl module Test::More unavailable, skipping test'
	test_done
}

# ----------------------------------------------------------------------

# The external test will outputs its own plan
test_external_has_tap=1

test_external \
	'GitwebCache::CacheOutput Perl API (in gitweb/lib/)' \
	"$PERL_PATH" "$TEST_DIRECTORY"/t9512/test_cache_output.pl

test_done
