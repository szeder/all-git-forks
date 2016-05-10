#!/bin/sh

test_description='Basic plumbing support of submodule--helper

This test verifies the submodule--helper plumbing command used to implement
git-submodule.
'

. ./test-lib.sh


test_expect_success 'valid-label-name tests empty label' '
	test_must_fail git submodule--helper valid-label-name 2>actual &&
	test_i18ngrep alphanumeric actual &&
	test_must_fail git submodule--helper valid-label-name "" 2>actual &&
	test_i18ngrep alphanumeric actual
'

test_expect_success 'valid-label-name tests correct label asdf' '
	git submodule--helper valid-label-name asdf 2>actual &&
	test_must_be_empty actual
'

test_expect_success 'valid-label-name tests correct label a' '
	git submodule--helper valid-label-name a 2>actual &&
	test_must_be_empty actual
'

test_expect_success 'valid-label-name tests correct label a-b' '
	git submodule--helper valid-label-name a-b 2>actual &&
	test_must_be_empty actual
'

test_expect_success 'valid-label-name fails with multiple arguments' '
	test_must_fail git submodule--helper valid-label-name a b 2>actual &&
	test_i18ngrep alphanumeric actual
'

test_expect_success 'valid-label-name fails with white spaced arguments' '
	test_must_fail git submodule--helper valid-label-name "a b" 2>actual &&
	test_i18ngrep alphanumeric actual
'

test_expect_success 'valid-label-name fails with utf8 characters' '
	test_must_fail git submodule--helper valid-label-name ☺ 2>actual &&
	test_i18ngrep alphanumeric actual
'

test_done
