#!/bin/sh

test_description='diff --color-words'

. ./test-lib.sh
. ../diff-lib.sh

dotest() {
	test_expect_success "$1" "
	echo '$1' >t &&
	git diff --color-words | tail -1 >actual &&
	cat actual &&
	test_cmp ../t4030/$2 actual
"	
}

test_expect_success 'setup for foo bar(_baz' '
	git config diff.nonwordchars "_()" &&
	echo "foo bar(_baz" > t &&
	git add t  &&
	git commit -m "add t"
'

dotest "foo bar(_" expect1
dotest "foo bar(" expect2
dotest "foo bar" expect3
dotest "foo (_baz" expect4
dotest "foo _baz" expect5
dotest "foo baz" expect6
dotest "bar(_baz" expect7

test_expect_success 'setup for foo bar(_' '
	echo "foo bar(_" > t &&
	git add t  &&
	git commit -m "add t"
'

dotest "foo bar(" expect8
dotest "foo bar" expect9
dotest "foo bar_baz" expect10

test_done
