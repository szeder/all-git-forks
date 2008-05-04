#!/bin/bash

git config diff.nonwordchars "_()"

echo "foo bar(_baz" >&2 &&
echo "foo bar(_baz" > t

git add t  &&
git commit -m "add t"

dotest() {
	echo "$1" >&2 &&
	echo "$1" >t  &&
	git diff --color-words |
	tail -1 > $2
}

dotest "foo bar(_" expect1
dotest "foo bar(" expect2
dotest "foo bar" expect3
dotest "foo (_baz" expect4
dotest "foo _baz" expect5
dotest "foo baz" expect6
dotest "bar(_baz" expect7


echo "foo bar(_" >&2 &&
echo "foo bar(_" >t

git add t  &&
git commit -m "add t"

dotest "foo bar(" expect8
dotest "foo bar" expect9
dotest "foo bar_baz" expect10
