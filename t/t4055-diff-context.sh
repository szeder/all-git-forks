#!/bin/sh
#
# Copyright (c) 2012 Mozilla Foundation
#

test_description='diff.context configuration'

. ./test-lib.sh

test_expect_success 'setup' '
	cat >x <<-EOF &&
firstline
b
c
d
e
f
preline
postline
i
j
k
l
m
n
EOF
	git update-index --add x &&
	git commit -m initial &&
	cat >x <<-\EOF &&
firstline
b
c
d
e
f
preline
1
postline
i
j
k
l
m
n
EOF
	git update-index --add x &&
	git commit -m next &&
cat >x <<-\EOF
firstline
b
c
d
e
f
preline
2
postline
i
j
k
l
m
n
EOF
'

test_expect_success 'diff.context affects log' '
	git log -1 -p >output &&
	! grep firstline output &&
	git config diff.context 8 &&
	git log -1 -p >output &&
	grep firstline output
'

test_expect_success 'different -U value' '
	git config diff.context 8 &&
	git log -U4 -1 >output &&
	! grep firstline output
'

test_expect_success 'diff.context affects diff' '
	git config diff.context 8 &&
	git diff >output &&
	grep firstline output
'

test_expect_success 'plumbing not affected' '
	git config diff.context 8 &&
	git diff-files -p > output &&
	! grep firstline output
'
test_expect_success 'non-integer config parsing' '
	cat > .git/config <<-\EOF &&
[diff]
	context = no
EOF
	! git diff 2>output &&
	grep "bad config value" output
'

test_expect_success 'negative integer config parsing' '
	cat >.git/config <<-\EOF &&
[diff]
	context = -1
EOF
	! git diff 2>output &&
	grep "bad config file" output
'

test_expect_success '0 config parsing' '
	cat > .git/config <<-\EOF &&
[diff]
	context = 0
EOF
	git diff >output &&
	grep preline output
'

test_expect_success 'config parsing' '
	cat >.git/config <<-\EOF &&
[diff]
	context = 8
EOF
	git diff >output &&
	grep postline output
'

test_done
