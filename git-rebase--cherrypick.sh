#!/bin/sh
#
# Copyright (c) 2010 Junio C Hamano.
#

GIT_CHERRY_PICK_HELP="$resolvemsg"
export GIT_CHERRY_PICK_HELP

case "$action" in
continue)
	# do we have anything to commit?
	if ! git diff-index --cached --quiet HEAD --
	then
		git commit --no-verify -e ||
			die "Could not commit staged changes."
	fi
	git cherry-pick --continue &&
	move_to_original_branch
	return
	;;
skip)
	git cherry-pick --skip &&
	move_to_original_branch
	return
	;;
esac

test -n "$rebase_root" && root_flag=--root

mkdir -p "$state_dir" || die "Could not create temporary $state_dir"
: > "$state_dir"/cherrypick || die "Could not mark as cherrypick"

if test -n "$rebase_root"
then
	revisions="$onto...$orig_head"
else
	revisions="$upstream...$orig_head"
fi

if test -n "$keep_empty"
then
	extra="--allow-empty"
else
	extra="--skip-empty --cherry-pick"
fi
test -n "$GIT_QUIET" && extra="$extra -q"
test -z "$force_rebase" && extra="$extra --ff"
git cherry-pick --no-merges --right-only --topo-order --do-walk --action-name rebase $extra "$revisions"
ret=$?

if test 0 != $ret
then
	write_basic_state
	return $ret
fi

move_to_original_branch
