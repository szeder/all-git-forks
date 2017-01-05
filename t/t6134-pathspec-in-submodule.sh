#!/bin/sh

test_description='test case exclude pathspec'

TEST_CREATE_SUBMODULE=yes
. ./test-lib.sh

test_expect_success 'setup a submodule' '
	git submodule add ./pretzel.bare sub &&
	git commit -a -m "add submodule" &&
	git submodule deinit --all
'

cat <<EOF >expect
fatal: Pathspec 'sub/a' is in submodule 'sub'
EOF

test_expect_success 'error message for path inside submodule' '
	echo a >sub/a &&
	test_must_fail git add sub/a 2>actual &&
	test_cmp expect actual
'

cat <<EOF >expect
fatal: Pathspec '.' is in submodule 'sub'
EOF

test_expect_success 'error message for path inside submodule from within submodule' '
	test_must_fail git -C sub add . 2>actual &&
	test_cmp expect actual
'

test_done
