#!/bin/sh

test_description='push from/to a shallow clone'

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

test_expect_success 'push from shallow clone' '
	(
	cd shallow &&
	test_commit 5 &&
	git push ../.git +master:refs/remotes/shallow/master
	) &&
	git log --format=%s shallow/master >actual &&
	git fsck &&
	cat <<EOF >expect &&
5
4
3
2
1
EOF
	test_cmp expect actual
'

test_expect_success 'push from shallow clone, with grafted roots' '
	git init shallow2 &&
	(
	cd shallow2 &&
	test_commit a &&
	test_commit b &&
	test_commit c &&
	git rev-parse b > .git/shallow &&
	git log --format=%s >actual &&
	cat <<EOF >expect &&
c
b
EOF
	test_cmp expect actual &&
	git push ../.git +master:refs/remotes/shallow2/master
	) &&
	git log --format=%s shallow2/master >actual &&
	git fsck &&
	cat <<EOF >expect &&
c
b
EOF
	test_cmp expect actual
'

test_expect_success 'push from shallow to shallow' '
	(
	cd shallow &&
	git push ../shallow2/.git +master:refs/remotes/shallow/master
	) &&
	(
	cd shallow2 &&
	git log --format=%s shallow/master >actual &&
	git fsck &&
	cat <<EOF >expect &&
5
4
3
EOF
	test_cmp expect actual
	)
'

test_done
