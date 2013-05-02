#!/bin/sh

die () {
	echo "$*" >&2
	exit 1
}

test $# = 1 || die "expected a single argument: $*"
test -n "$GIT_TEST_MSGSTORE_MESSAGE" || die "need GIT_TEST_MSGSTORE_MESSAGE set"

if test -n "$GIT_TEST_MSGSTORE_MESSAGEID"
then
	test "$1" = "$GIT_TEST_MSGSTORE_MESSAGEID" ||
	die "incorrect message ID: $1 != $GIT_TEST_MSGSTORE_MESSAGEID"
fi

echo "$GIT_TEST_MSGSTORE_MESSAGE"
