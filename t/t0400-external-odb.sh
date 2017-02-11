#!/bin/sh

test_description='basic tests for external object databases'
. ./test-lib.sh

ALT_SOURCE=$PWD/alt-repo/.git
export ALT_SOURCE
write_script odb-helper <<\EOF
GIT_DIR=$ALT_SOURCE; export GIT_DIR
case "$1" in
have)
	git rev-list --all --objects |
	cut -d' ' -f1 |
	git cat-file --batch-check |
	awk '{print $1 " " $3 " " $2}'
	;;
get)
	cat "$GIT_DIR"/objects/$(echo $2 | sed 's#..#&/#')
	;;
esac
EOF
HELPER="\"$PWD\"/odb-helper"

test_expect_success 'setup alternate repo' '
	git init alt-repo &&
	(cd alt-repo &&
	 test_commit one &&
	 test_commit two
	) &&
	alt_head=`cd alt-repo && git rev-parse HEAD`
'

test_expect_success 'alt objects are missing' '
	test_must_fail git log --format=%s $alt_head
'

test_expect_success 'helper can retrieve alt objects' '
	test_config odb.magic.command "$HELPER" &&
	cat >expect <<-\EOF &&
	two
	one
	EOF
	git log --format=%s $alt_head >actual &&
	test_cmp expect actual
'

test_done
