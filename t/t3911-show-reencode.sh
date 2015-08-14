#!/bin/sh

test_description='reencoding'

. ./test-lib.sh

printf '\304\201\n' >a_macron_utf8
printf '\303\244\n' >a_diaeresis_utf8
printf '\303\244\304\n' >incomplete_utf8
printf '\344\n' >a_diaeresis_latin1

test_expect_success 'setup' '
	git commit --allow-empty -F a_diaeresis_utf8 &&
	git tag latin1_utf8 &&
	git commit --allow-empty -F a_macron_utf8 &&
	git tag extended_utf8 &&
	git commit --allow-empty -F incomplete_utf8 &&
	git tag invalid_utf8
'

test_expect_success 'encoding to latin1' '
	git log --encoding=latin1 --pretty=format:%B -1 latin1_utf8 >out 2>err &&
	test_must_be_empty err &&
	test_cmp out a_diaeresis_latin1
'

test_expect_success 'unknown encoding' '
	git log --encoding=no-encoding --pretty=format:%B -1 latin1_utf8 >out 2>err &&
	grep -q "not supported" err &&
	test_cmp out a_diaeresis_utf8
'

# apparently incomplete UTF8 byte sequences silently treated as latin1
test_expect_failure 'incomplete utf8' '
	git log --encoding=latin1 --pretty=format:%B -1 invalid_utf8 >out 2>err &&
	grep -q "Invalid input" err &&
	test_cmp out incomplete_utf8
'

test_expect_success 'does not fit into latin1' '
	git log --encoding=latin1 --pretty=format:%B -1 extended_utf8 >out 2>err &&
	grep -q "Invalid input" err &&
	test_cmp out a_macron_utf8
'

test_done
