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
