#!/bin/sh
USAGE='[help|list|merge|rebase|pivot|create|update]'
LONG_USAGE='
git work
    Print the range that defines the boundaries of the branch
git work list [commit|dependency]
    List the commits or dependencies of the current working branch.
git work merge [dependency ...]
    Merge the specified dependencies into the base of the branch
git work pivot [pivot]
    Swap two segments of the working branch at the commit specified by the pivot
git work rebase [-i]
    Invoke an interactive rebase on the branch
git work rebase [dependency]
    Rebase the current working branch onto the specified dependency.
git work create [--pivot-first] topic [pivot] [on-base]
    Rebase the top N commits onto new topic based on a specified commit, then merge the result into the base of the current branch
git work update [--pivot-first] topic [pivot]
    Rebase the top N commits onto an existing topic, then merge the updated topic into the base of the current branch
git work help
    Print this help

Please use "git help work" to get the full man page.'

SUBDIRECTORY_OK=true
OPTIONS_SPEC="
$LONG_USAGE
--
pivot-first perform a pivot first, then do..
as-refs print the range as references
X pass-thru options to merge or rebase
i,interactive perform an interactive rebase
"
. git-sh-setup
. git-atomic-lib

warn()
{
    echo "warn: $*" 1>&2
}

not_implemented()
{
    die "not yet implemented"
}

work_default()
{
   if $AS_REFS
   then
       echo $BASEREF..$BRANCH
   else
       echo $(git base)..$(git rev-parse HEAD)
   fi
}

work_list_dependency()
{
    limits="$*"
    while true
    do
	 top=$(git rev-list $BASE $limits --no-merges --max-count=1)
	 if test -z "$top"
	 then
	     break;
	 else
	     echo $top
	     limits="$limits ^$top"
	 fi
    done
}

work_list()
{
    type=$1
    test $# -gt 0 && shift
    case "$type" in
       ""|commit)
	    git rev-list $(work_default)
       ;;
       dependency)
	    work_list_dependency "$@"
       ;;
       *)
	    die "$type is not a supported type for this command"
       ;;
    esac
}

work_merge()
{
   DEPENDENCY=$1
   shift

   test -n "${DEPENDENCY}" || die "usage: git work merge [dependency [merge-options]]"

   assert --not-staged --not-unstaged --commit-exists "${DEPENDENCY}"

   atomic eval \
"
	DEPENDENCY=${DEPENDENCY} &&
	BASE=${BASE} &&
	BASEREF=${BASEREF} &&
	BRANCH=${BRANCH} &&
	MERGE_OPTIONS='${MERGE_OPTIONS}'
	git checkout -q \${BASE} &&
	git merge --no-ff -q \${DEPENDENCY} \${MERGE_OPTIONS} &&
	MERGE=\$(git rev-parse HEAD) &&
	git rebase --onto HEAD \${BASE} \${BRANCH} &&
	git update-ref \${BASEREF} \${MERGE}
"
}

work_unmerge()
{
    assert --not-unstaged --not-staged --not-detached

    work_list dependency | sort > .git/git-work.unmerge.list

    for arg; do
	git rev-parse --verify "$arg" 2>/dev/null
    done | sort  > .git/git-work.unmerge.args

    saved=$(diff -u .git/git-work.unmerge.list .git/git-work.unmerge.args | tail -n +4 | grep "^[+|-]" | while read c
    do
	case $c in
	    +*)
		die "'${c#+}' is not a dependency."
		;;
	    -*)
		git rev-parse --verify "${c#-}" 2>/dev/null || die "'$c' is not a commit."
		;;
	esac
    done | tr \\012 ' ') || exit $?

    test -n "$saved" || die "You must preserve at least one dependency."

    atomic eval "
	set -- $saved;
	git checkout \$1 || die 'Failed to checkout a merge commit.';
	shift;

	while test \$# -gt 0
	do
		git merge \$1 || die \"Failed to merge '\$1'\";
		shift;
	done &&
	NEWBASE=\$(git rev-parse HEAD) &&
	git rebase --onto HEAD $BASE $BRANCH &&
	git base set \$NEWBASE
"
}

work_rebase()
{
   git base -q check || die "use 'git base' to establish a base for this branch"
   if test -n "${INTERACTIVE}"
   then
	PIVOT=${1:-$(git base)}
	git base -q check ${PIVOT} || die "${PIVOT} is not a valid pivot commit"
	git rebase -i ${PIVOT} ${BRANCH} "$@"
   else

      DEPENDENCY=$1

      assert --not-staged --not-unstaged --commit-exists "${DEPENDENCY}"
      atomic eval "
      git rebase --onto \${DEPENDENCY} \${BASE} \${BRANCH} &&
      git update-ref \${BASEREF} \${DEPENDENCY}
"
   fi
}

work_create()
{
   TOPIC=$1
   PIVOT=$2
   DEPENDENCY=$3

   test -n "${DEPENDENCY}" || die "usage: git work create topic [pivot [dependency]]"

   PIVOT=$(git rev-parse --verify "$PIVOT" 2>/dev/null) || die "$2 is not a commit"

   git base check "${PIVOT}" > /dev/null || die "$PIVOT does not lie between $BASE and $HEAD"

   assert --commit-exists "${DEPENDENCY}" --not-branch-exists "${TOPIC}" --not-staged --not-unstaged

   if $PIVOT_FIRST
   then
   atomic eval "
	BASE=${BASE} &&
	TOPIC=${TOPIC} &&
	PIVOT=${PIVOT} &&
	HEAD=${HEAD} &&
	BRANCHREF=${BRANCHREF} &&
	DEPENDENCY=${DEPENDENCY} &&
	git rebase -q --onto \${DEPENDENCY} \${BASE} \${PIVOT} &&
	git branch \${TOPIC} &&
	git base -q -b \${TOPIC} set \${DEPENDENCY} &&
	git rebase -q --onto \${BASE} \${PIVOT} \${BRANCH} &&
	git work merge \${TOPIC}
"
   else
   atomic eval "
	TOPIC=${TOPIC} &&
	PIVOT=${PIVOT} &&
	HEAD=${HEAD} &&
	BRANCHREF=${BRANCHREF} &&
	DEPENDENCY=${DEPENDENCY} &&
	git rebase -q --onto \${DEPENDENCY} \${PIVOT} \${HEAD} &&
	git branch \${TOPIC} &&
	git base -b \${TOPIC} set \${DEPENDENCY} &&
	git update-ref \${BRANCHREF} \${PIVOT} &&
	git checkout \${BRANCH} &&
	git work merge \${TOPIC}
"
   fi

}

work_update()
{
   TOPIC=$1
   PIVOT=$2

   test -n "${PIVOT}" || die "usage: git work update topic [pivot [dependency]]"

   PIVOT=$(git rev-parse --verify "$PIVOT" 2>/dev/null) || die "$2 is not a commit"

   git base check "${PIVOT}" > /dev/null || die "$PIVOT does not lie between $BASE and $HEAD"

   assert --branch-exists "${TOPIC}" --not-staged --not-unstaged

   if $PIVOT_FIRST
   then
   atomic eval "
	TOPIC=${TOPIC} &&
	PIVOT=${PIVOT} &&
	BASE=${BASE} &&
	HEAD=${HEAD} &&
	BRANCHREF=${BRANCHREF} &&
	git base -q -b \${TOPIC} \${TOPIC} &&
	git rebase -q --onto \${TOPIC} \${BASE} \${PIVOT} &&
	git branch -f \${TOPIC} &&
	git rebase -q --onto \${BASE} \${PIVOT} \${BRANCH} &&
	git work merge \${TOPIC}
"
   else
   atomic eval "
	TOPIC=${TOPIC} &&
	PIVOT=${PIVOT} &&
	HEAD=${HEAD} &&
	BRANCHREF=${BRANCHREF} &&
	git base -q -b \${TOPIC} \${TOPIC} &&
	git rebase -q --onto \${TOPIC} \${PIVOT} \${HEAD} &&
	git branch -f \${TOPIC} &&
	git update-ref \${BRANCHREF} \${PIVOT} &&
	git checkout \${BRANCH} &&
	git work merge \${TOPIC}
"
   fi

}

work_pivot()
{
   PIVOT=$1

   test -n "${PIVOT}" || die "usage: git work update topic [pivot [dependency]]"

   PIVOT=$(git rev-parse --verify "$PIVOT" 2>/dev/null) || die "$1 is not a commit"

   git base check "${PIVOT}" > /dev/null || die "$PIVOT does not lie between $BASE and $HEAD"

   assert --not-staged --not-unstaged

   atomic eval "
      git rebase -q --onto \${BASE} \${PIVOT} \${HEAD} &&
      git rebase -q --onto HEAD \${BASE} \${PIVOT} &&
      git update-ref \${BRANCHREF} HEAD &&
      git checkout -q \${BRANCH}
"
}

INTERACTIVE=
CURRENT_BRANCH=$(git branch | grep "^\* [^(]" | cut -c3-)
AS_REFS=false
PIVOT_FIRST=false
if test -n "$CURRENT_BRANCH"
then
   BRANCHREF=refs/heads/${CURRENT_BRANCH}
   BASEREF=refs/bases/${CURRENT_BRANCH}
   BRANCH=${CURRENT_BRANCH}
else
   BRANCHREF=HEAD
   BASEREF=BASE
   BRANCH=HEAD
fi

while test $# != 0
do
	case $1 in
		-b)
		shift
		BRANCHREF=refs/heads/$1
		BASEREF=refs/bases/$1
		shift
		(git rev-parse --verify $BRANCHREF 1>/dev/null) || die "$1 is not a valid local branch"
		;;
		-i)
			INTERACTIVE=-i
			shift
		;;
		--as-refs)
		shift
		AS_REFS=true
		;;
		--pivot-first)
		shift
		PIVOT_FIRST=true
		;;
		--)
		shift
		break;
		;;
		*)
		break;
		;;
	esac
done


# ensure that a base is established.
git base -q || die "Please use 'git base init' to initialise the reset command for this branch."

HEAD=$(git rev-parse --verify ${BRANCHREF} 2>/dev/null)
BASE=$(git rev-parse --verify ${BASEREF} 2>/dev/null)

test -n "$BASE" || die "can't derive BASE from BASEREF=${BASEREF}"

case "$#" in
0)
    work_default "$@" ;;
*)
    cmd="$1"
    shift
    if test $cmd = 'trace'; then set -x; cmd=$1; shift; fi
    case "$cmd" in
    help)
	    git work -h "$@" ;;
    list|merge|rebase|pivot|create|update|unmerge)
	    work_$cmd "$@" ;;
    *)
	    usage "$@" ;;
    esac
esac
