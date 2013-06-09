#!/bin/sh
#
# Copyright (c) 2010 Junio C Hamano.
#

case "$action" in
continue)
	git am --resolved --resolvemsg="$resolvemsg" &&
	move_to_original_branch
	return
	;;
skip)
	git am --skip --resolvemsg="$resolvemsg" &&
	move_to_original_branch
	return
	;;
esac

test -n "$rebase_root" && root_flag=--root

git cherry-pick --allow-empty "$revisions"
ret=$?

if test 0 != $ret
then
	test -d "$state_dir" && write_basic_state
	return $ret
fi

move_to_original_branch
