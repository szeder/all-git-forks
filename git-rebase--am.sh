#!/bin/sh
#
# Copyright (c) 2010 Junio C Hamano.
#

case "$action" in
continue)
	git am --resolved --resolvemsg="$resolvemsg" &&
	move_to_original_branch
	exit
	;;
skip)
	git am --skip --resolvemsg="$resolvemsg" &&
	move_to_original_branch
	exit
	;;
esac

test -n "$keep_empty" && git_am_opt="$git_am_opt --keep-empty"
generate_revisions |
sed -e 's/\([0-9a-f]\{40\}\)/From \1 Mon Sep 17 00:00:00 2001/' |
git am $git_am_opt --rebasing --resolvemsg="$resolvemsg" &&
move_to_original_branch

ret=$?
test 0 != $ret -a -d "$state_dir" && write_basic_state
exit $ret
