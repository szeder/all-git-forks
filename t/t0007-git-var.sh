#!/bin/sh

test_description='basic sanity checks for git var'
. ./test-lib.sh

test_expect_success 'get GIT_AUTHOR_IDENT' '
	test_tick &&
	echo "A U Thor <author@example.com> 1112911993 -0700" >expect &&
	git var GIT_AUTHOR_IDENT >actual &&
	test_cmp expect actual
'

test_expect_success 'get GIT_COMMITTER_IDENT' '
	test_tick &&
	echo "C O Mitter <committer@example.com> 1112912053 -0700" >expect &&
	git var GIT_COMMITTER_IDENT >actual &&
	test_cmp expect actual
'

test_expect_success 'git var can show multiple values' '
	cat >expect <<-\EOF &&
	A U Thor <author@example.com> 1112912053 -0700
	C O Mitter <committer@example.com> 1112912053 -0700
	EOF
	git var GIT_AUTHOR_IDENT GIT_COMMITTER_IDENT >actual &&
	test_cmp expect actual
'

test_done
