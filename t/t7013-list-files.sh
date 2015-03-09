#!/bin/sh

test_description='list-files'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir sa sa/sb sc &&
	touch a b c sa/a sa/sb/b sc/c &&
	git add .
'

test_expect_success 'list-files -R from index' '
	git list-files -R >actual &&
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

test_expect_success 'list-files from index' '
	git list-files --max-depth=0 >actual &&
	cat >expect <<-\EOF &&
	a
	b
	c
	sa
	sc
	EOF
	test_cmp expect actual &&
	git list-files >actual &&
	test_cmp expect actual
'

test_expect_success 'column output' '
	COLUMNS=20 git list-files -R --column=always >actual &&
	cat >expected <<-\EOF &&
	a        sa/a
	b        sa/sb/b
	c        sc/c
	EOF
	test_cmp expected actual &&
	COLUMNS=20 git -c column.listfiles=always list-files -R >actual &&
	cat >expected <<-\EOF &&
	a        sa/a
	b        sa/sb/b
	c        sc/c
	EOF
	test_cmp expected actual &&
	COLUMNS=20 git -c column.listfiles=always list-files -1R >actual &&
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
	git list-files -R "*a" >actual &&
	cat >expect <<-\EOF &&
	a
	sa/a
	EOF
	test_cmp expect actual
'

test_expect_success '--max-depth' '
	git list-files --max-depth=1 >actual &&
	cat >expected <<-\EOF &&
	a
	b
	c
	sa/a
	sa/sb
	sc/c
	EOF
	test_cmp expected actual
'

test_expect_success 'list-files from subdir ' '
	(
	cd sa &&
	git list-files -R >actual &&
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
	git list-files -R ../a sb >actual &&
	cat >expect <<-\EOF &&
	../a
	sb/b
	EOF
	test_cmp expect actual
	)
'

test_expect_success 'setup 2' '
	git init 2 &&
	(
	cd 2 &&
	mkdir dir &&
	touch file dir/file &&
	git init gitlink &&
	( cd gitlink && test_commit foo ) &&
	git add file dir/file gitlink &&
	git commit -qm1
	)
'

test_expect_success 'LS_COLORS env variable' '
	(
	cd 2 &&
	LS_COLORS="rs=0:fi=31:di=32" &&
	export LS_COLORS &&
	git list-files --color=always | test_decode_color | \
		grep -v gitlink >actual &&
	cat >expected <<-\EOF &&
	<GREEN>dir<RESET>
	<RED>file<RESET>
	EOF
	test_cmp expected actual
	)
'

test_expect_success 'color.ls.*' '
	(
	cd 2 &&
	test_config color.ls.file red &&
	test_config color.ls.directory green &&
	test_config color.ls.submodule yellow &&
	git list-files --color=always | test_decode_color >actual &&
	cat >expected <<-\EOF &&
	<GREEN>dir<RESET>
	<RED>file<RESET>
	<YELLOW>gitlink<RESET>
	EOF
	test_cmp expected actual
	)
'

test_done
