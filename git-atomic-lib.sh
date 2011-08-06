#!/bin/sh
#
# Provides a function that provides for robust recovery from
#
. git-test-lib
atomic()
{
    assert --not-conflicted --message "cannot perform an atomic operation while there are merge conflicts"
    HEAD=$(git rev-parse --verify HEAD) || setup_failed "failed to resolve HEAD"
    if REF=$(git symbolic-ref -q HEAD)
    then
	  BRANCH=${REF#refs/heads/}
    else
	BRANCH=${HEAD}
    fi

    STASH=$(git stash create) || setup_failed "failed to stash"
    REF=$(git rev-parse --symbolic-full-name HEAD) || setup_failed "failed to acquire REF"
    REBASE_DIR=$(git rev-parse --git-dir)/rebase-apply
    REBASE_COUNT=1
    test_condition -q --rebasing || REBASE_COUNT=0

    (
      "$@"
    ) || (
       RC=$?

       command_failed()
       {
	   rc=$1
	   shift
	   echo "command failed: $* rc=$rc" 1>&2
	   exit 1
       }

       restore_failed()
       {
	   echo "restore failed: $*" 1>&2
	   exit 2
       }

       if test $REBASE_COUNT -eq 0 && test -d "$REBASE_DIR"
       then
	    git rebase --abort || restore_failed "failed to abort rebase"
       fi

	{
		git reset --hard HEAD &&
		git checkout -q ${BRANCH}
	} || restore_failed "failed to checkout ${BRANCH}"

	if test -n "$STASH"
	then
		git stash apply --index "$STASH" || restore_failed "failed to reapply stash $STASH to $HEAD"
		echo "restored $REF to $(git describe --always --abbrev=6 $HEAD), reapplied stash $(git describe --always --abbrev=6 $STASH)" 1>&2
	fi
	command_failed $RC "$*"
    )
}
