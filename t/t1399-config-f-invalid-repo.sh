#!/bin/sh

test_description='test git config -f works in invalid repo'
. ./test-lib.sh

test_expect_success setup '
	rm -fr .git && touch .git
'

file=`mktemp`

test_expect_failure 'call git-config -f' 'git config -f "$file" core.bare false'

test_done
