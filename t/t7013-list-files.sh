#!/bin/sh

test_description='list-files'

. ./test-lib.sh

test_expect_success 'setup' '
	mkdir sa sa/sb sc &&
	touch a b c sa/a sa/sb/b sc/c &&
	git add a sa/a &&
	git commit -m initial &&
	git add . &&
	echo foo >.git/info/exclude &&
	touch foo bar sa/foo sa/bar
'

test_expect_success 'list-files --others' '
	git list-files --others >actual &&
	cat >expect <<-\EOF &&
	?? actual
	?? bar
	   sa
	EOF
	test_cmp expect actual &&
	git list-files --others --cached >actual &&
	cat >expect <<-\EOF &&
	   a
	?? actual
	   b
	?? bar
	   c
	?? expect
	   sa
	   sc
	EOF
	test_cmp expect actual &&
	git list-files --others -R >actual &&
	cat >expect <<-\EOF &&
	actual
	bar
	expect
	sa/bar
	EOF
	test_cmp expect actual
'

test_expect_success 'list-files --others' '
	git list-files --ignored >actual &&
	cat >expect <<-\EOF &&
	!! foo
	   sa
	EOF
	test_cmp expect actual &&
	git list-files --ignored -R >actual &&
	cat >expect <<-\EOF &&
	foo
	sa/foo
	EOF
	test_cmp expect actual
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
	test_cmp expect actual &&
	git list-files --cached >actual &&
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

test_expect_success '--classify' '
	(
	cd 2 &&
	git list-files -F >actual &&
	cat >expected <<-\EOF &&
	dir/
	file
	gitlink&
	EOF
	test_cmp expected actual
	)
'

test_expect_success 'list-files unmerged' '
	(
	add_stage() {
		echo "100644 $1 $2	$3" | git update-index --index-info
	}
	git init 3 &&
	cd 3 &&
	test_commit 1 &&
	SHA1=`echo 1 | git hash-object -w --stdin` &&
	add_stage $SHA1 1 deleted-by-both &&
	add_stage $SHA1 2 added-by-us &&
	add_stage $SHA1 1 deleted-by-them &&
	add_stage $SHA1 2 deleted-by-them &&
	add_stage $SHA1 3 added-by-them &&
	add_stage $SHA1 3 deleted-by-us &&
	add_stage $SHA1 1 deleted-by-us &&
	add_stage $SHA1 2 added-by-both &&
	add_stage $SHA1 3 added-by-both &&
	add_stage $SHA1 1 modified-by-both &&
	add_stage $SHA1 2 modified-by-both &&
	add_stage $SHA1 3 modified-by-both &&
	git list-files -u >actual &&
	cat >expected <<-\EOF &&
	AA added-by-both
	UA added-by-them
	AU added-by-us
	DD deleted-by-both
	UD deleted-by-them
	DU deleted-by-us
	UU modified-by-both
	EOF
	test_cmp expected actual
	)
'

test_done
