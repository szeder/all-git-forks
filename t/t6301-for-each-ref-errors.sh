#!/bin/sh

test_description='for-each-ref errors for broken refs'

. ./test-lib.sh

ZEROS=0000000000000000000000000000000000000000
MISSING=abababababababababababababababababababab

test_expect_success setup '
	git commit --allow-empty -m "Initial" &&
	git tag testtag &&
	git for-each-ref >full-list
'

test_expect_success 'Broken refs are reported correctly' '
	r=refs/heads/bogus &&
	: >.git/$r &&
	test_when_finished "rm -f .git/$r" &&
	echo "warning: ignoring broken ref $r" >broken-err &&
	git for-each-ref >out 2>err &&
	test_cmp full-list out &&
	test_cmp broken-err err
'

test_expect_failure 'NULL_SHA1 refs are reported correctly' '
	r=refs/heads/zeros &&
	echo $ZEROS >.git/$r &&
	test_when_finished "rm -f .git/$r" &&
	echo "warning: ignoring broken ref $r" >zeros-err &&
	git for-each-ref >out 2>err &&
	test_cmp full-list out &&
	test_cmp zeros-err err
'

test_expect_success 'Missing objects are reported correctly' '
	r=refs/heads/missing &&
	echo $MISSING >.git/$r &&
	test_when_finished "rm -f .git/$r" &&
	echo "fatal: missing object $MISSING for $r" >missing-err &&
	test_must_fail git for-each-ref 2>err &&
	test_cmp missing-err err
'

test_done
