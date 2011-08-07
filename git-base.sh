#!/bin/sh
#
# (c) Copyright Jon Seymour 2010
#
USAGE='[help|set|check|reset|init]'
LONG_USAGE='
git base [--as-ref]
	print the SHA1 hash of the base reference or its symbolic name (--as-ref)
git base set [-f] <commit>
	set the base of the branch to commit nearest <commit> that satisifies the base invariant.
git base clear
	As per set, but never update the base reference.
git base check <commit>
	test if the specified commit satisfies the base invariant
git base init <reset-cmd>
	Initialise the git base command to be called when git base reset is invoked.
git base reset
	Invoke the configured reset command (see git base init) or git base clear, if not such command is configured.
git base help
	print this help

Please use "git help base" to get the full man page.'

OPTIONS_SPEC=

SUBDIRECTORY_OK=true
. "$(git --exec-path)/git-sh-setup"
require_work_tree

warn()
{
    test -n "$QUIET" || echo "$*" 1>&2
}

info()
{
    test -n "$QUIET" || echo "$*" 1>&2
}

data()
{
	if test "$*" != ""
	then
		echo "$*"
		true
	else
		false
	fi
}

quietly()
{
	if test -n "$QUIET"
	then
		"$@" >/dev/null 2>/dev/null
	else
		"$@"
	fi
}

assert_valid_branch()
{
	test -n "${VALID_BRANCH}" || die "${BRANCH} is not a branch"
}

invariant_state()
{
	commit=$1
	if test -z "$commit"
	then
		echo "UNDEFINED"
		false
	elif ! git rev-parse --quiet --verify "$commit" >/dev/null
	then
		echo "INVALID"
		false
	elif ! git test -q --reachable $commit $HEAD
	then
		echo "UNREACHABLE"
		false
	elif ! test -z "$(last_merge $HEAD $commit)"
	then
		echo "HIDDEN"
		false
	else
		echo "CONSISTENT"
		true
	fi
}

closest()
{
	commit=$1
	state=${2:-$(invariant_state $commit)}

	case $state in
		UNDEFINED|INVALID)
			:
		;;
		UNREACHABLE)
			closest $(git merge-base $HEAD $commit)
		;;
		HIDDEN)
			last_merge $HEAD $commit
		;;
		CONSISTENT)
			echo $commit
		;;
	esac
}

describe()
{
	commit=$1
	state=$2

	case $state in
		UNDEFINED)
			echo "No commit specified."
		;;
		INVALID)
			echo "$commit is not a valid reference."
		;;
		HIDDEN)
			echo "The commit $(short_ref $commit) is not a base of ${BRANCH} because it is hidden by the merge commit $(short_ref $(last_merge $HEAD $commit))."
		;;
		UNREACHABLE)
			echo "The commit $(short_ref $commit) is not a base of ${BRANCH} because it is unreachable from $(short_ref $HEAD)."
		;;
		CONSISTENT)
			echo "The commit $(short_ref $commit) satisfies the base invariant of $(short_ref $HEAD)."
		;;
	esac
}

last_merge()
{
    head=$1
    commit=$2
    data $(git rev-list --max-count=1 --merges ${head} ^$commit)
}

short_ref()
{
    data "($(git rev-parse --short $1))"
}

base_default()
{
	assert_valid_branch

	revs=$(git rev-parse --revs-only "$@")

	if state=$(invariant_state "${BASEREF}")
	then
		if test -z "$ASREF"
		then
			git rev-parse ${BASEREF}
		else
			echo "${BASEREF}"
		fi
	else
		if test -z "$ASREF"
		then
			if test -n "$revs"
			then
				base_set $revs && return 0
			fi

			describe "${BASEREF}" $state 1>&2
			base_reset
		else
			describe "${BASEREF}" $state 1>&2
			echo "${BASEREF}"
			false
		fi
	fi
}

base_init()
{
	if test -n "$DELETE"
	then
		git update-ref -d ${BASEREF} &&
		git config branch.${BRANCH}.baseresetcmd clear &&
		git config --unset branch.${BRANCH}.baseresetcmd
		return 0
	fi

	assert_valid_branch

	if test $# -eq 0
	then
		set -- $(git config branch.${BRANCH}.merge)
		MERGE=$1
		set -- $(git config branch.${BRANCH}.remote)
		case "$1" in
		.)
			OPTION="set ${MERGE}"
		;;
		"")
			OPTION="set $(git rev-parse ${BRANCH})"
		;;
		*)
			OPTION="set $1/${MERGE#refs/heads/}"
		;;
		esac
		set -- ${OPTION}
	else
		case "$1" in
			set|clear|check)
			:
			;;
		*)
			die "$1 is not a valid 'git base' command"
			;;
		esac
	fi

	warn "The reset command for ${BRANCH} is now '$*'."
	git config branch.${BRANCH}.baseresetcmd "$*" || die "failed to update branch.${BRANCH}.baseresetcmd"
}

base_reset()
{
	assert_valid_branch

	options=$(git config branch.${BRANCH}.baseresetcmd)
	options=${options:-clear}

	warn "Resetting the base of ${BRANCH} with 'git base ${options}'."
	git base -b ${BRANCH} ${options}
}

base_check()
{
	assert_valid_branch

	specified=$1
	commit=${specified:-${BASEREF}}

	if state=$(invariant_state "$commit")
	then
		git rev-parse $commit
	else
		describe "$commit" $state 1>&2
		false
	fi
}

base_set()
{
	assert_valid_branch

	USAGE="usage: git base set [-f] <commit>"
	test -n "$1" || die "$USAGE"

	commit=$1

	if state=$(invariant_state "$commit")
	then
		git update-ref "${BASEREF}" "$commit" || die "failed to update $BASEREF"
		echo $(git rev-parse $commit)
	else

		case $state in
			INVALID)
				die "The specified reference $commit is not a valid."
			;;
			HIDDEN|UNREACHABLE)
				closest=$(closest $commit)
				describe "$commit" "$state" 1>&2
				if test -z "$FORCE"
				then
					warn "Updating the base of ${BRANCH} to a consistent value $(short_ref $closest)."
					git update-ref ${BASEREF} $closest || die "failed to update $BASEREF"
					echo $closest
					true
				else
					warn "Updating the base of ${BRANCH} to an inconsistent value $(short_ref $commit)."
					git update-ref ${BASEREF} $commit || die "failed to update $BASEREF"
					false
				fi
			;;
			CONSISTENT)
				git update-ref ${BASEREF} $commit || die "failed to update $BASEREF"
				echo $commit
				true
			;;
			*)
				die "should never happen - invalid state $state"
			;;
		esac

	fi
}

base_clear()
{
   git update-ref -d ${BASEREF} >/dev/null || die "failed to clear $BASEREF"
   warn "The base of ${BRANCH} has been cleared."
   false
}

base_help()
{
   git base -h "$@"
}

VALID_BRANCH=
QUIET=
POSITIONAL=
FORCE=
DELETE=

BRANCHREF=$(git symbolic-ref -q HEAD) || BRANCHREF=HEAD
BRANCH=${BRANCHREF#refs/heads/}
VALID_BRANCH=t

while test $# -gt 0
do
	arg=$1
	shift

	case "$arg" in
	       -b)
			test -n "$1" || die "-b requires a branch to be specified"
			BRANCH=$1 && shift
			BRANCHREF=$(git rev-parse --quiet --symbolic-full-name --verify "${BRANCH}" --)
			BRANCH=${BRANCHREF#refs/heads/}
			test "${BRANCH}" = "${BRANCHREF}" || VALID_BRANCH=t
			test "${BRANCH}" = "HEAD" && VALID_BRANCH=t
			test -n "${BRANCHREF}"  || VALID_BRANCH=
		;;
		-f)
			FORCE=-f;
		;;
		-d)
			DELETE=-d;
		;;
		-q)
			QUIET=-q
		;;
		--)
			break;
		;;
		--as-ref)
			ASREF=--as-ref;
		;;
		default|check|clear|init|reset|set|help)
			if test -z "$CMD"
			then
				CMD=$arg
			else
				POSITIONAL="${POSITIONAL}${POSITIONAL:+ }$arg"
			fi
		;;
		*)
			POSITIONAL="${POSITIONAL}${POSITIONAL:+ }$arg"
		;;
	esac
done

CMD=${CMD:-default}

set -- $POSITIONAL

if test "${BRANCH}" = "HEAD"
then
	BASEREF=BASE
else
	BASEREF=refs/bases/${BRANCH}
fi

HEAD=$(git rev-parse --quiet --verify ${BRANCHREF})
BASE=$(git rev-parse --quiet --verify ${BASEREF})

quietly base_$CMD "$@"
