#!/bin/sh

test_description='index file specific tests'

. ./test-lib.sh

test_expect_success 'setup' '
	echo 1 >a
'

test_expect_success 'out of bounds GIT_INDEX_VERSION issues warning' '
	(
		GIT_INDEX_VERSION=1 &&
		export GIT_INDEX_VERSION &&
		git add a 2>&1 | sed "s/[0-9]//" >actual.err &&
		sed -e "s/ Z$/ /" <<-\EOF >expect.err &&
			warning: GIT_INDEX_VERSION set, but the value is invalid.
			Using version Z
		EOF
		test_i18ncmp expect.err actual.err
	)
'

test_done
