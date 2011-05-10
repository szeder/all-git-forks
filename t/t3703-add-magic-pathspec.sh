#!/bin/sh

test_description='magic pathspec tests using git-add'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir sub anothersub &&
	: >sub/foo &&
	: >anothersub/foo
'

test_expect_success 'colon alone magic should only used alone' '
	test_must_fail git add -n sub/foo : &&
	test_must_fail git add -n : sub/foo
'

test_expect_success 'add :' '
	: >expected &&
	(cd sub && git add -n : >actual) &&
	test_cmp expected sub/actual
'

test_expect_success 'add :/' "
	cat >expected <<-EOF &&
	add 'anothersub/foo'
	add 'expected'
	add 'sub/actual'
	add 'sub/foo'
	EOF
	(cd sub && git add -n :/ >actual) &&
	test_cmp expected sub/actual
"

cat >expected <<EOF
add 'anothersub/foo'
EOF

test_expect_success 'add :/anothersub' '
	(cd sub && git add -n :/anothersub >actual) &&
	test_cmp expected sub/actual
'

test_expect_success 'add :/non-existent' '
	(cd sub && test_must_fail git add -n :/non-existent)
'

cat >expected <<EOF
add 'sub/foo'
EOF

test_expect_success 'add :(icase)foo' '
	(cd sub && git add -n ":(icase)FoO" >actual) &&
	test_cmp expected sub/actual
'

test_expect_success 'a file with the same (long) magic name exists' '
	: >":(icase)ha" &&
	test_must_fail git add -n ":(icase)ha" 2>error &&
	git add -n "./:(icase)ha"
'

cat >expected <<EOF
fatal: pathspec ':(icase)ha' did not match any files
EOF

test_expect_failure 'show pathspecs exactly what are typed in' '
	test_cmp expected error
'

test_expect_success 'a file with the same (short) magic name exists' '
	mkdir ":" &&
	: >":/bar" &&
	test_must_fail git add -n :/bar &&
	git add -n "./:/bar"
'

test_done
