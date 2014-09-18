#!/bin/sh

# This script is run via make.
# "make check-headers SKIP_HEADER_CHECK=1" skips the header dependency check.
# "make check-headers SKIP_USAGE_CHECK=1" skips the header usage check.

exit_code=0

maybe_exit () {
	status="$1"
	if test "$status" != 0
	then
		exit_code="$status"
		if test -n "$CHECK_HEADERS_STOP"
		then
			exit "$status"
		fi
	fi
}

check_header () {
	header="$1"
	shift
	case "$header" in
	common-cmds.h)
		# should only be included by help.c, not checked
		;;
	*)
		subdir=$(dirname "$header") &&
		echo "HEADER $header" &&
		"$@" -Wno-unused -I"$subdir" -include git-compat-util.h \
			-c -o "$header".check -x c - <"$header" &&
		rm "$header".check ||
		maybe_exit $?
		;;
	esac
}

check_headers () {
	for header in *.h ewah/*.h vcs-svn/*.h xdiff/*.h
	do
		check_header "$header" "$@"
	done
}

check_header_usage () {
	first=$(sed -n -e '/^#include/{
			s/#include ["<]\(.*\)".*/\1/p
			q
		}' "$1")
	case "$first" in
	cache.h|builtin.h|git-compat-util.h)
		# happy
		;;
	*)
		echo "error: $1 must #include \"git-compat-util.h\" before $first"
		maybe_exit 1
		;;
	esac

	if grep common-cmds.h "$1" >/dev/null && test "$1" != help.c
	then
		echo "error: $1 must not include common-cmds.h"
		maybe_exit 1
	fi
}

check_usage () {
	# Implementation files should #include git-compat-util.h, cache.h,
	# or builtin.h before any others.
	for impl in *.c builtin/*.c
	do
		check_header_usage "$impl"
	done
}

main () {
	if test -z "$SKIP_HEADER_CHECK"
	then
		check_headers "$@"
	fi

	if test -z "$SKIP_USAGE_CHECK"
	then
		check_usage
	fi

	exit $exit_code
}

main "$@"
