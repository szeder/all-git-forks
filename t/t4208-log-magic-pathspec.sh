#!/bin/sh

test_description='magic pathspec tests using git-log'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit initial &&
	test_tick &&
	git commit --allow-empty -m empty &&
	mkdir sub
'

test_expect_failure '"git log :/" should be ambiguous' '
	test_must_fail git log :/ 2>error &&
	grep ambiguous error
'

test_expect_failure '"git log :" should be ambiguous' '
	test_must_fail git log : 2>error &&
	grep ambiguous error
'

test_expect_success 'git log -- :' '
	git log -- :
'

test_expect_success 'git log HEAD -- :/' '
	cat >expected <<-EOF &&
	24b24cf initial
	EOF
	(cd sub && git log --oneline HEAD -- :/ >../actual) &&
	test_cmp expected actual
'

test_expect_success 'git log HEAD -- :' '
	cat >expected <<-EOF &&
	41d179c empty
	24b24cf initial
	EOF
	(cd sub && git log --oneline HEAD -- : >../actual) &&
	test_cmp expected actual
'

test_done
