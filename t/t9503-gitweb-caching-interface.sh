#!/bin/sh
#
# Copyright (c) 2010 Jakub Narebski
#

test_description='gitweb caching interface

This test checks caching interface used in gitweb caching, and caching
infrastructure (GitwebCache::* modules).'

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
	'GitwebCache::* Perl API (in gitweb/lib/)' \
	"$PERL_PATH" "$TEST_DIRECTORY"/t9503/test_cache_interface.pl

test_done
