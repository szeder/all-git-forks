#!/bin/sh

test_description='check handling of SIGPIPE'
. ./test-lib.sh

test_expect_success 'create blob' '
	test-genrandom foo 16384 >file &&
	git add file
'

large_git () {
	for i in $(test_seq 1 100); do
		git diff --staged --binary || return $?
	done
}

test_expect_success 'git dies with SIGPIPE' '
	OUT=$( ((large_git; echo $? 1>&3) | true) 3>&1 )
	test "$OUT" -eq 141
'

test_expect_success 'git dies with SIGPIPE even if parent ignores it' '
	OUT=$( ((trap "" PIPE; large_git; echo $? 1>&3) | true) 3>&1 )
	test "$OUT" -eq 141
'

test_done
