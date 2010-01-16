#!/bin/sh
#
# Copyright (c) 2010 Jakub Narebski
#

test_description='caching interface to be used in gitweb'
#test_description='caching interface used in gitweb, gitweb caching
#
#This test checks cache (interface) used in gitweb caching, caching
#infrastructure and gitweb response (output) caching (the last by
#running gitweb as CGI script from commandline).'

# for now we are running only cache interface tests
. ./test-lib.sh

# this test is present in gitweb-lib.sh
if ! test_have_prereq PERL; then
	say 'perl not available, skipping test'
	test_done
fi

"$PERL_PATH" -MTest::More -e 0 >/dev/null 2>&1 || {
	say 'perl module Test::More unavailable, skipping test'
	test_done
}

# ----------------------------------------------------------------------

test_external 'GitwebCache::* Perl API (in gitweb/cache.pm)' \
	"$PERL_PATH" "$TEST_DIRECTORY"/t9503/test_cache_interface.pl

test_done
