#!/bin/sh
#
# Copyright (c) 2016 Jacob Keller
#

test_description='Basic plumbing support of submodule--helper

This test verifies the submodule--helper plumbing command used to implement
git-submodule.
'

. ./test-lib.sh

test_expect_success 'sanitize-config clears configuration' '
	git -c user.name="Some User" submodule--helper sanitize-config >actual &&
	test_must_be_empty actual
'

test_expect_success 'sanitize-config keeps credential.helper' '
	git -c credential.helper="helper" submodule--helper sanitize-config >actual &&
	echo "'\''credential.helper=helper'\''" >expect &&
	test_cmp expect actual
'

test_done
