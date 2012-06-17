#!/bin/sh

test_description='calculate and cache commit generations'
. ./test-lib.sh

test_expect_success 'setup history' '
	test_commit one &&
	test_commit two &&
	test_commit three &&
	test_commit four &&
	git checkout -b other two &&
	test_commit five &&
	git checkout master &&
	git merge other &&
	test_commit six
'

cat >expect <<'EOF'
5 six
4 Merge branch 'other'
2 five
3 four
2 three
1 two
0 one
EOF
test_expect_success 'check commit generations' '
	git log --format="%G %s" >actual &&
	test_cmp expect actual
'

test_expect_success 'cache file was created' '
	test_path_is_file .git/cache/generations
'

test_expect_success 'cached values are the same' '
	git log --format="%G %s" >actual &&
	test_cmp expect actual
'

test_done
