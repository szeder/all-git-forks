#!/bin/sh

test_description='fetch/clone from a shallow clone'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit 1 &&
	test_commit 2 &&
	test_commit 3 &&
	test_commit 4
'

test_expect_success 'setup shallow clone' '
	git clone --no-local --depth=2 .git shallow &&
	git --git-dir=shallow/.git log --format=%s >actual &&
	cat <<EOF >expect &&
4
3
EOF
	test_cmp expect actual
'

test_expect_success 'clone from shallow clone' '
	git clone --no-local shallow shallow2 &&
	(
	cd shallow2 &&
	git fsck &&
	git log --format=%s >actual &&
	cat <<EOF >expect &&
4
3
EOF
	test_cmp expect actual
	)
'

test_expect_success 'fetch from shallow clone' '
	(
	cd shallow &&
	test_commit 5
	) &&
	(
	cd shallow2 &&
	git fetch &&
	git fsck &&
	git log --format=%s origin/master >actual &&
	cat <<EOF >expect &&
5
4
3
EOF
	test_cmp expect actual
	)
'

test_expect_success 'fetch --depth from shallow clone' '
	(
	cd shallow &&
	test_commit 6
	) &&
	(
	cd shallow2 &&
	git fetch --depth=2 &&
	git fsck &&
	git log --format=%s origin/master >actual &&
	cat <<EOF >expect &&
6
5
EOF
	test_cmp expect actual
	)
'

test_done
