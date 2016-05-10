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

test_expect_success 'setup superproject with submodules' '

	mkdir sub &&
	(
		cd sub &&
		git init &&
		test_commit test
		test_commit test2
	) &&
	mkdir super &&
	(
		cd super &&
		git init &&
		git submodule add ../sub sub0 &&
		git submodule add -l bit1 ../sub sub1 &&
		git submodule add -l bit2 ../sub sub2 &&
		git submodule add -l bit2 -l bit1 ../sub sub3 &&
		git submodule add ../sub sub_name &&
		git mv sub_name sub_path &&
		git commit -m "add labeled submodules"
	)
'

test_expect_success 'in-group' '
	(
		cd super &&
		# we do not specify a group nor have set a default group,
		# any submodule should be in the default group:
		git submodule--helper in-group sub0 &&
		git submodule--helper in-group sub1 &&
		git submodule--helper in-group sub2 &&
		git submodule--helper in-group sub3 &&

		# test bit1:
		test_must_fail git submodule--helper in-group --group=\*bit1 sub0 &&
			       git submodule--helper in-group --group=\*bit1 sub1 &&
		test_must_fail git submodule--helper in-group --group=\*bit1 sub2 &&
			       git submodule--helper in-group --group=\*bit1 sub3 &&
		test_must_fail git submodule--helper in-group --group=\*bit1 sub_path &&

		# test by path:
			       git submodule--helper in-group --group=./sub0 sub0 &&
		test_must_fail git submodule--helper in-group --group=./sub0 sub1 &&
		test_must_fail git submodule--helper in-group --group=./sub0 sub_path &&

		# tests by name:
			       git submodule--helper in-group --group=:sub0 sub0 &&
		test_must_fail git submodule--helper in-group --group=:sub0 sub1 &&
			       git submodule--helper in-group --group=:sub_name sub_path &&

		# logical OR of path and labels
			       git submodule--helper in-group --group=\*bit1,./sub0 sub0 &&
			       git submodule--helper in-group --group=\*bit1,./sub0 sub1 &&
		test_must_fail git submodule--helper in-group --group=\*bit1,./sub0 sub2 &&
			       git submodule--helper in-group --group=\*bit1,./sub0 sub3
	)
'

test_expect_success 'submodule init respects label' '
	test_when_finished "rm -rf super_clone" &&
	suburl=$(pwd)/sub &&
	git clone super super_clone &&
	(
		cd super_clone &&
		git submodule init \*bit1 &&
		test_must_fail git config submodule.sub0.url &&
		test        "$(git config submodule.sub1.url)" = "$suburl" &&
		test_must_fail git config submodule.sub2.url &&
		test        "$(git config submodule.sub3.url)" = "$suburl"
	)
'

test_expect_success 'submodule init respects label' '
	test_when_finished "rm -rf super_clone" &&
	suburl=$(pwd)/sub &&
	git clone super super_clone &&
	(
		cd super_clone &&
		git submodule init :sub_name &&
		test_must_fail git config submodule.sub0.url &&
		test "$(git config submodule.sub_name.url)" = "$suburl"
	)
'

test_done
