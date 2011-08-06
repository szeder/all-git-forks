#!/bin/sh
#
# (c) Copyright Jon Seymour 2010
#
if test -z "${__GIT_ASSERT_LIB_INCLUDED}"
then
__GIT_ASSERT_LIB_INCLUDED=t

. $(git --exec-path)/git-conditions-lib

is_a_function()
{
    test "$(type $1 2>/dev/null | sed -n "s/is a .*function/is a function/p")" = "$1 is a function"
}

if ! is_a_function die
then
die() {
	echo >&2 "$@"
	exit 1
}
fi

require_lib()
{
	if test -f "$1"
	then
		. "$1" || die "failed to read: '$1'"
	else
		echo "warning: condition library '$1' does not exist" 1>&2
	fi
}

require_condition_libs() {
	eval $(
		git config --get-all condition.lib | while read lib
		do
			echo "require_lib \"$lib\" \;"
		done
	)
}

assertion_failed() {
	rc=$1
	shift
	message="${MESSAGE:-$*}"
	if ! ${QUIET:-false}
	then
		echo "${message}" 1>&2
	fi

	if test -z "${EXIT_ON_FAILURE}"
	then
		return $rc
	else
		exit $rc;
	fi
}

evaluation_failed() {
	rc=$1
	shift
	echo "fatal: condition evaluation failed for $*" 1>&2
	exit $rc
}

not() {
	! "$@"
}

#
# reviewers: is there a more concise way to express this in POSIX?
#
replace() {
	word=$1
	from=$2
	to=$3

	prefix=${word%${from}*}
	suffix=${word#${prefix}${from}}

	if test "$prefix" = "$word"
	then
		echo $word
	else
		echo "$(replace ${prefix} '-' '_')${to}${suffix}"
	fi
}

impl() {
	MESSAGE=
	QUIET=false
	QUEUE=""
	exprs=""
	while test $# -gt 0
	do
		word=$1
		shift
		case $word in
			--message)
			test $# -gt 0 || die "--message requires the following argument to be a message"
			MESSAGE=$1
			shift
			continue
			;;
			--include)
				test $# -gt 0 || die "can't shift 1 argument for --include option"
				test -f "$1" || die "'$1' must be a file"
				require_lib "$1"
				shift
				continue
			;;
			-q)
			QUIET=true
			continue;
			;;
			--not-*)
			negation='not'
			condition=${word#--not-}
			;;
			--*)
			negation=''
			condition=${word#--}
			;;
			*)
			die "argument not recognised: $word"
			;;
		esac

		dehyphenated=$(replace "$condition" "-" "_")

		expr=
		args=
		try=0

		while ! is_a_function check_${dehyphenated}_$try
		do
			test -n "$1" || die "condition $condition is not supported or insufficient arguments were supplied"
			test "${1#--}" = "$1" || die "condition $condition is not supported or insufficient arguments were supplied"
			args="${args}${args:+ }$(git rev-parse --sq-quote $1)"
			shift
			try=$((try+1))
		done

		exprs="${exprs}${exprs:+ }${negation}${negation:+ }$word $try check_${dehyphenated}_$try $args"

	done

	set -- $exprs
	while test $# -gt 0
	do
		if test "$1" = "not"
		then
			negation=not
			shift
		else
			negation=
		fi
		word=$1
		nargs=$2
		shift 2
		message=$(eval $negation "$@")
		rc=$?
		if test $rc -ne 0
		then
			if test -n "$message"
			then
				assertion_failed $rc "$message"
			else
				evaluation_failed $rc "$word $2 ..."
			fi
			return $rc
		fi
		shift $((nargs+1))
	done
}

assert() {
	EXIT_ON_FAILURE=t
	impl "$@"
}

test_condition() {
	EXIT_ON_FAILURE=
	impl "$@" 2>/dev/null
}

fi
