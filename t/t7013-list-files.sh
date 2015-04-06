#!/bin/sh

test_description='list-files'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir sa sa/sb sc &&
	touch a b c sa/a sa/sb/b sc/c &&
	git add .
'

test_expect_success 'list-files from index' '
	git list-files >actual &&
	cat >expect <<-\EOF &&
	a
	b
	c
	sa/a
	sa/sb/b
	sc/c
	EOF
	test_cmp expect actual
'

test_expect_success 'column output' '
	COLUMNS=20 git list-files --column=always >actual &&
	cat >expected <<-\EOF &&
	a        sa/a
	b        sa/sb/b
	c        sc/c
	EOF
	test_cmp expected actual &&
	COLUMNS=20 git -c column.listfiles=always list-files >actual &&
	cat >expected <<-\EOF &&
	a        sa/a
	b        sa/sb/b
	c        sc/c
	EOF
	test_cmp expected actual &&
	COLUMNS=20 git -c column.listfiles=always list-files -1 >actual &&
	cat >expected <<-\EOF &&
	a
	b
	c
	sa/a
	sa/sb/b
	sc/c
	EOF
	test_cmp expected actual
'

test_expect_success 'list-files selectively from index' '
	git list-files "*a" >actual &&
	cat >expect <<-\EOF &&
	a
	sa/a
	EOF
	test_cmp expect actual
'

test_expect_success 'list-files from subdir ' '
	(
	cd sa &&
	git list-files >actual &&
	cat >expect <<-\EOF &&
	a
	sb/b
	EOF
	test_cmp expect actual
	)
'

test_expect_success 'list-files from subdir (2)' '
	(
	cd sa &&
	git list-files ../a sb >actual &&
	cat >expect <<-\EOF &&
	../a
	sb/b
	EOF
	test_cmp expect actual
	)
'

test_done
