#!/bin/sh
#
# Copyright (c) 2010 Junio C Hamano.
#

case "$action" in
continue)
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

git cherry-pick --allow-empty "$revisions"
ret=$?

if test 0 != $ret
then
	write_basic_state
	return $ret
fi

move_to_original_branch
