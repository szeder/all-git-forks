#!/bin/sh

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

for header in *.h ewah/*.h vcs-svn/*.h xdiff/*.h
do
	case "$header" in
	common-cmds.h)
		# should only be included by help.c
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
done

exit $exit_code
