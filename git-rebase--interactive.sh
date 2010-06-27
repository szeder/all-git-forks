#!/bin/sh
#
# Copyright (c) 2006 Johannes E. Schindelin

# SHORT DESCRIPTION
#
# This script makes it easy to fix up commits in the middle of a series,
# and rearrange commits.
#
# The original idea comes from Eric W. Biederman, in
# http://article.gmane.org/gmane.comp.version-control.git/22407

OPTIONS_KEEPDASHDASH=
OPTIONS_SPEC="\
git-rebase [-i] [options] [--] <upstream> [<branch>]
git-rebase [-i] (--continue | --abort | --skip)
--
 Available options are
v,verbose          display a diffstat of what changed upstream
onto=              rebase onto given branch instead of upstream
p,preserve-merges  try to recreate merges instead of ignoring them
s,strategy=        use the given merge strategy
no-ff              cherry-pick all commits, even if unchanged
m,merge            always used (no-op)
i,interactive      always used (no-op)
 Actions:
continue           continue rebasing process
abort              abort rebasing process and restore original branch
skip               skip current patch and continue rebasing process
no-verify          override pre-rebase hook from stopping the operation
verify             allow pre-rebase hook to run
root               rebase all reachable commmits up to the root(s)
autosquash         move commits that begin with squash!/fixup! under -i
"

. git-sh-setup
require_work_tree

DOTEST="$GIT_DIR/rebase-merge"

# The file containing rebase commands, comments, and empty lines.
# This file is created by "git rebase -i" then edited by the user.  As
# the lines are processed, they are removed from the front of this
# file and written to the tail of $DONE.
TODO="$DOTEST"/git-rebase-todo

# The rebase command lines that have already been processed.  A line
# is moved here when it is first handled, before any associated user
# actions.
DONE="$DOTEST"/done

# The commit message that is planned to be used for any changes that
# need to be committed following a user interaction.
MSG="$DOTEST"/message

# The file into which is accumulated the suggested commit message for
# squash/fixup commands.  When the first of a series of squash/fixups
# is seen, the file is created and the commit message from the
# previous commit and from the first squash/fixup commit are written
# to it.  The commit message for each subsequent squash/fixup commit
# is appended to the file as it is processed.
#
# The first line of the file is of the form
#     # This is a combination of $COUNT commits.
# where $COUNT is the number of commits whose messages have been
# written to the file so far (including the initial "pick" commit).
# Each time that a commit message is processed, this line is read and
# updated.  It is deleted just before the combined commit is made.
SQUASH_MSG="$DOTEST"/message-squash

# If the current series of squash/fixups has not yet included a squash
# command, then this file exists and holds the commit message of the
# original "pick" commit.  (If the series ends without a "squash"
# command, then this can be used as the commit message of the combined
# commit without opening the editor.)
FIXUP_MSG="$DOTEST"/message-fixup

# $REWRITTEN is the name of a directory containing files for each
# commit that is reachable by at least one merge base of $HEAD and
# $UPSTREAM. They are not necessarily rewritten, but their children
# might be.  This ensures that commits on merged, but otherwise
# unrelated side branches are left alone. (Think "X" in the man page's
# example.)
# $(cat "$REWRITTEN"/$ORIGINAL_SHA1) = $REWRITTEN_SHA1
REWRITTEN="$DOTEST"/rewritten

# $MARK is a directory which contains a file named after each mark set
# by the 'mark' rebase command, containing the sha1 of the marked commit.
MARK="$DOTEST"/mark

# $MARK_CONSTRUCT is used in generate_script.  It names a directory
# which contains a file named after each commit for which a mark is to
# be emitted.  The file contains the name of the mark, and is empty if
# no such mark has yet been emitted.  The files are named by short sha1.
MARK_CONSTRUCT="$DOTEST"/mark-construct

# A script to set the GIT_AUTHOR_NAME, GIT_AUTHOR_EMAIL, and
# GIT_AUTHOR_DATE that will be used for the commit that is currently
# being rebased.
AUTHOR_SCRIPT="$DOTEST"/author-script

# When an "edit" rebase command is being processed, the SHA1 of the
# commit to be edited is recorded in this file.  When "git rebase
# --continue" is executed, if there are any staged changes then they
# will be amended to the HEAD commit, but only provided the HEAD
# commit is still the commit to be edited.  When any other rebase
# command is processed, this file is deleted.
AMEND="$DOTEST"/amend

# For the post-rewrite hook, we make a list of rewritten commits and
# their new sha1s.  The rewritten-pending list keeps the sha1s of
# commits that have been processed, but not committed yet,
# e.g. because they are waiting for a 'squash' command.
REWRITTEN_LIST="$DOTEST"/rewritten-list
REWRITTEN_PENDING="$DOTEST"/rewritten-pending

PRESERVE_MERGES=
STRATEGY=
ONTO=
VERBOSE=
OK_TO_SKIP_PRE_REBASE=
REBASE_ROOT=
AUTOSQUASH=
test "$(git config --bool rebase.autosquash)" = "true" && AUTOSQUASH=t
NEVER_FF=

GIT_CHERRY_PICK_HELP="\
hint: after resolving the conflicts, mark the corrected paths
hint: with 'git add <paths>' and run 'git rebase --continue'"
export GIT_CHERRY_PICK_HELP

warn () {
	printf '%s\n' "$*" >&2
}

output () {
	case "$VERBOSE" in
	'')
		output=$("$@" 2>&1 )
		status=$?
		test $status != 0 && printf "%s\n" "$output"
		return $status
		;;
	*)
		"$@"
		;;
	esac
}

# Output the commit message for the specified commit.
commit_message () {
	git cat-file commit "$1" | sed "1,/^$/d"
}

run_pre_rebase_hook () {
	if test -z "$OK_TO_SKIP_PRE_REBASE" &&
	   test -x "$GIT_DIR/hooks/pre-rebase"
	then
		"$GIT_DIR/hooks/pre-rebase" ${1+"$@"} || {
			echo >&2 "The pre-rebase hook refused to rebase."
			exit 1
		}
	fi
}


ORIG_REFLOG_ACTION="$GIT_REFLOG_ACTION"

comment_for_reflog () {
	case "$ORIG_REFLOG_ACTION" in
	''|rebase*)
		GIT_REFLOG_ACTION="rebase -i ($1)"
		export GIT_REFLOG_ACTION
		;;
	esac
}

peek_next_command () {
	sed -n -e "/^#/d" -e '/^$/d' -e "s/ .*//p" -e "q" < "$TODO"
}

# expects the original commit name(s) in "$REWRITTEN"/original
# records the current HEAD as the rewritten commit
add_rewritten () {
	test ! -d "$REWRITTEN" && return
	for original in $(cat "$REWRITTEN"/original)
	do
		original=$(git rev-parse --verify "$original") &&
		rewritten=$(git rev-parse --verify HEAD) &&
		echo $rewritten > "$REWRITTEN"/$original || break
	done &&
	case "$(peek_next_command)" in
	squash|s) ;; # do nothing
	*) rm "$REWRITTEN"/original;;
	esac ||
	die "Could not store information about rewritten commit"
}

last_count=
mark_action_done () {
	sed -e 1q < "$TODO" >> "$DONE"
	sed -e 1d < "$TODO" >> "$TODO".new
	mv -f "$TODO".new "$TODO"
	count=$(sane_grep -c '^[^#]' < "$DONE")
	total=$(($count+$(sane_grep -c '^[^#]' < "$TODO")))
	if test "$last_count" != "$count"
	then
		last_count=$count
		printf "Rebasing (%d/%d)\r" $count $total
		test -z "$VERBOSE" || echo
	fi
}

make_patch () {
	sha1_and_parents="$(git rev-list --parents -1 "$1")"
	case "$sha1_and_parents" in
	?*' '?*' '?*)
		git diff --cc $sha1_and_parents
		;;
	?*' '?*)
		git diff-tree -p "$1^!"
		;;
	*)
		echo "Root commit"
		;;
	esac > "$DOTEST"/patch
	test -f "$MSG" ||
		commit_message "$1" > "$MSG"
	test -f "$AUTHOR_SCRIPT" ||
		get_author_ident_from_commit "$1" > "$AUTHOR_SCRIPT"
}

die_with_patch () {
	echo "$1" > "$DOTEST"/stopped-sha
	make_patch "$1"
	git rerere
	die "$2"
}

die_abort () {
	rm -rf "$DOTEST"
	die "$1"
}

has_action () {
	sane_grep '^[^#]' "$1" >/dev/null
}

# Run command with GIT_AUTHOR_NAME, GIT_AUTHOR_EMAIL, and
# GIT_AUTHOR_DATE exported from the current environment.
do_with_author () {
	(
		export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL GIT_AUTHOR_DATE
		"$@"
	)
}

pick_one () {
	ff=--ff
	case "$1" in -n) sha1=$2; ff= ;; *) sha1=$1 ;; esac
	case "$NEVER_FF" in '') ;; ?*) ff= ;; esac
	test -d "$REWRITTEN" && echo "$sha1" >> "$REWRITTEN"/original
	output git rev-parse --verify $sha1 || die "Invalid commit name: $sha1"
	if test -n "$REBASE_ROOT"
	then
		output git cherry-pick "$@"
		return
	fi
	output git cherry-pick $ff "$@"
}

merge_one () {
	cmd="merge $*"
	test "$1" = parents && shift
	parents=
	while test "original" != "$1"
	do
		parents="$parents $(parse_commit $1)"
		shift
	done

	test "original" != "$1" &&
		die "Could not determine original merge commit from $cmd"

	sha1=$2; shift; shift

	# the command was "merge parents ...", so "parents" was recorded
	echo "$sha1" > "$REWRITTEN"/original &&
	git merge -m "$*" $parents &&
	add_rewritten ||
	die_with_patch $sha1 "Could not redo merge $sha1 with parents $parents"
	echo "$sha1 $(git rev-parse HEAD^0)" >> "$REWRITTEN_LIST"
}

nth_string () {
	case "$1" in
	*1[0-9]|*[04-9]) echo "$1"th;;
	*1) echo "$1"st;;
	*2) echo "$1"nd;;
	*3) echo "$1"rd;;
	esac
}

update_squash_messages () {
	if test -f "$SQUASH_MSG"; then
		mv "$SQUASH_MSG" "$SQUASH_MSG".bak || exit
		COUNT=$(($(sed -n \
			-e "1s/^# This is a combination of \(.*\) commits\./\1/p" \
			-e "q" < "$SQUASH_MSG".bak)+1))
		{
			echo "# This is a combination of $COUNT commits."
			sed -e 1d -e '2,/^./{
				/^$/d
			}' <"$SQUASH_MSG".bak
		} >"$SQUASH_MSG"
	else
		commit_message HEAD > "$FIXUP_MSG" || die "Cannot write $FIXUP_MSG"
		COUNT=2
		{
			echo "# This is a combination of 2 commits."
			echo "# The first commit's message is:"
			echo
			cat "$FIXUP_MSG"
		} >"$SQUASH_MSG"
	fi
	case $1 in
	squash)
		rm -f "$FIXUP_MSG"
		echo
		echo "# This is the $(nth_string $COUNT) commit message:"
		echo
		commit_message $2
		;;
	fixup)
		echo
		echo "# The $(nth_string $COUNT) commit message will be skipped:"
		echo
		commit_message $2 | sed -e 's/^/#	/'
		;;
	esac >>"$SQUASH_MSG"
}

# A squash/fixup has failed.  Prepare the long version of the squash
# commit message, then die_with_patch.  This code path requires the
# user to edit the combined commit message for all commits that have
# been squashed/fixedup so far.  So also erase the old squash
# messages, effectively causing the combined commit to be used as the
# new basis for any further squash/fixups.  Args: sha1 rest
die_failed_squash() {
	mv "$SQUASH_MSG" "$MSG" || exit
	rm -f "$FIXUP_MSG"
	cp "$MSG" "$GIT_DIR"/MERGE_MSG || exit
	warn
	warn "Could not apply $1... $2"
	die_with_patch $1 ""
}

flush_rewritten_pending() {
	test -s "$REWRITTEN_PENDING" || return
	newsha1="$(git rev-parse HEAD^0)"
	sed "s/$/ $newsha1/" < "$REWRITTEN_PENDING" >> "$REWRITTEN_LIST"
	rm -f "$REWRITTEN_PENDING"
}

record_in_rewritten() {
	oldsha1="$(git rev-parse $1)"
	echo "$oldsha1" >> "$REWRITTEN_PENDING"

	case "$(peek_next_command)" in
	squash|s|fixup|f)
		;;
	*)
		flush_rewritten_pending
		;;
	esac
}

parse_commit () {
	name="$1"
	case "$name" in
	:*)
		cat "$MARK"/"$name" || die "Unknown mark: $name"
		;;
	*)
		git rev-parse --verify "$name" || die "Invalid commit name: $sha1"
		;;
	esac
}

do_next () {
	rm -f "$MSG" "$AUTHOR_SCRIPT" "$AMEND" || exit
	read -r command sha1 rest < "$TODO"
	case "$command" in
	'#'*|''|noop)
		mark_action_done
		;;
	pick|p)
		comment_for_reflog pick

		mark_action_done
		pick_one $sha1 && add_rewritten ||
			die_with_patch $sha1 "Could not apply $sha1... $rest"
		record_in_rewritten $sha1
		;;
	reword|r)
		comment_for_reflog reword

		mark_action_done
		pick_one $sha1 ||
			die_with_patch $sha1 "Could not apply $sha1... $rest"
		git commit --amend --no-post-rewrite
		record_in_rewritten $sha1
		;;
	edit|e)
		comment_for_reflog edit

		mark_action_done
		pick_one $sha1 ||
			die_with_patch $sha1 "Could not apply $sha1... $rest"
		echo "$sha1" > "$DOTEST"/stopped-sha
		make_patch $sha1
		git rev-parse --verify HEAD > "$AMEND"
		warn "Stopped at $sha1... $rest"
		warn "You can amend the commit now, with"
		warn
		warn "	git commit --amend"
		warn
		warn "Once you are satisfied with your changes, run"
		warn
		warn "	git rebase --continue"
		warn
		exit 0
		;;
	squash|s|fixup|f)
		case "$command" in
		squash|s)
			squash_style=squash
			;;
		fixup|f)
			squash_style=fixup
			;;
		esac
		comment_for_reflog $squash_style

		test -f "$DONE" && has_action "$DONE" ||
			die "Cannot '$squash_style' without a previous commit"

		mark_action_done
		update_squash_messages $squash_style $sha1
		author_script=$(get_author_ident_from_commit HEAD)
		echo "$author_script" > "$AUTHOR_SCRIPT"
		eval "$author_script"
		output git reset --soft HEAD^
		pick_one -n $sha1 || die_failed_squash $sha1 "$rest"
		case "$(peek_next_command)" in
		squash|s|fixup|f)
			# This is an intermediate commit; its message will only be
			# used in case of trouble.  So use the long version:
			do_with_author output git commit --no-verify -F "$SQUASH_MSG" ||
				die_failed_squash $sha1 "$rest"
			;;
		*)
			# This is the final command of this squash/fixup group
			if test -f "$FIXUP_MSG"
			then
				do_with_author git commit --no-verify -F "$FIXUP_MSG" ||
					die_failed_squash $sha1 "$rest"
			else
				cp "$SQUASH_MSG" "$GIT_DIR"/SQUASH_MSG || exit
				rm -f "$GIT_DIR"/MERGE_MSG
				do_with_author git commit --no-verify -e ||
					die_failed_squash $sha1 "$rest"
			fi
			rm -f "$SQUASH_MSG" "$FIXUP_MSG"
			;;
		esac
		add_rewritten
		record_in_rewritten $sha1
		;;
	x|"exec")
		read -r command rest < "$TODO"
		mark_action_done
		printf 'Executing: %s\n' "$rest"
		# "exec" command doesn't take a sha1 in the todo-list.
		# => can't just use $sha1 here.
		git rev-parse --verify HEAD > "$DOTEST"/stopped-sha
		${SHELL:-@SHELL_PATH@} -c "$rest" # Actual execution
		status=$?
		if test "$status" -ne 0
		then
			warn "Execution failed: $rest"
			warn "You can fix the problem, and then run"
			warn
			warn "	git rebase --continue"
			warn
			exit "$status"
		fi
		# Run in subshell because require_clean_work_tree can die.
		if ! (require_clean_work_tree "rebase")
		then
			warn "Commit or stash your changes, and then run"
			warn
			warn "	git rebase --continue"
			warn
			exit 1
		fi
		;;
	mark)
		mark_action_done
		echo "$sha1" | sane_egrep -q '^:[a-zA-Z0-9]+$' || \
			die "Invalid mark name: $sha1"
		mkdir -p "$MARK" &&
		git rev-parse HEAD >"$MARK"/"$sha1"
		;;
	goto)
		comment_for_reflog goto
		mark_action_done
		git reset --hard "$(parse_commit $sha1)" || \
			die "Failed to reset: $sha1"
		;;
	merge|m)
		comment_for_reflog merge
		mark_action_done
		# this already dies with patch on error
		merge_one $sha1 $rest # $sha1 is not really the sha1...
		;;
	*)
		warn "Unknown command: $command $sha1 $rest"
		if git rev-parse --verify -q "$sha1" >/dev/null
		then
			die_with_patch $sha1 "Please fix this in the file $TODO."
		else
			die "Please fix this in the file $TODO."
		fi
		;;
	esac
	test -s "$TODO" && return

	comment_for_reflog finish &&
	HEADNAME=$(cat "$DOTEST"/head-name) &&
	OLDHEAD=$(cat "$DOTEST"/head) &&
	SHORTONTO=$(git rev-parse --short $(cat "$DOTEST"/onto)) &&
	NEWHEAD=$(git rev-parse HEAD) &&
	case $HEADNAME in
	refs/*)
		message="$GIT_REFLOG_ACTION: $HEADNAME onto $SHORTONTO" &&
		git update-ref -m "$message" $HEADNAME $NEWHEAD $OLDHEAD &&
		git symbolic-ref HEAD $HEADNAME
		;;
	esac && {
		test ! -f "$DOTEST"/verbose ||
			git diff-tree --stat $(cat "$DOTEST"/head)..HEAD
	} &&
	{
		test -s "$REWRITTEN_LIST" &&
		git notes copy --for-rewrite=rebase < "$REWRITTEN_LIST" ||
		true # we don't care if this copying failed
	} &&
	if test -x "$GIT_DIR"/hooks/post-rewrite &&
		test -s "$REWRITTEN_LIST"; then
		"$GIT_DIR"/hooks/post-rewrite rebase < "$REWRITTEN_LIST"
		true # we don't care if this hook failed
	fi &&
	rm -rf "$DOTEST" &&
	git gc --auto &&
	warn "Successfully rebased and updated $HEADNAME."

	exit
}

do_rest () {
	while :
	do
		do_next
	done
}

# skip picking commits whose parents are unchanged
skip_unnecessary_picks () {
	fd=3
	while read -r command rest
	do
		# fd=3 means we skip the command
		case "$fd,$command" in
		3,pick|3,p)
			# pick a commit whose parent is current $ONTO -> skip
			sha1=${rest%% *}
			case "$(git rev-parse --verify --quiet "$sha1"^)" in
			"$ONTO"*)
				ONTO=$sha1
				;;
			*)
				fd=1
				;;
			esac
			;;
		3,#*|3,)
			# copy comments
			;;
		*)
			fd=1
			;;
		esac
		printf '%s\n' "$command${rest:+ }$rest" >&$fd
	done <"$TODO" >"$TODO.new" 3>>"$DONE" &&
	mv -f "$TODO".new "$TODO" &&
	case "$(peek_next_command)" in
	squash|s|fixup|f)
		record_in_rewritten "$ONTO"
		;;
	esac ||
	die "Could not skip unnecessary pick commands"
}

# check if no other options are set
is_standalone () {
	test $# -eq 2 -a "$2" = '--' &&
	test -z "$ONTO" &&
	test -z "$PRESERVE_MERGES" &&
	test -z "$STRATEGY" &&
	test -z "$VERBOSE"
}

get_saved_options () {
	test -d "$REWRITTEN" && PRESERVE_MERGES=t
	test -f "$DOTEST"/strategy && STRATEGY="$(cat "$DOTEST"/strategy)"
	test -f "$DOTEST"/verbose && VERBOSE=t
	test -f "$DOTEST"/rebase-root && REBASE_ROOT=t
}

# Rearrange the todo list that has both "pick sha1 msg" and
# "pick sha1 fixup!/squash! msg" appears in it so that the latter
# comes immediately after the former, and change "pick" to
# "fixup"/"squash".
rearrange_squash () {
	# extract fixup!/squash! lines and resolve any referenced sha1's
	while read -r pick sha1 message
	do
		case "$message" in
		"squash! "*|"fixup! "*)
			action="${message%%!*}"
			rest="${message#*! }"
			echo "$sha1 $action $rest"
			# if it's a single word, try to resolve to a full sha1 and
			# emit a second copy. This allows us to match on both message
			# and on sha1 prefix
			if test "${rest#* }" = "$rest"; then
				fullsha="$(git rev-parse -q --verify "$rest" 2>/dev/null)"
				if test -n "$fullsha"; then
					# prefix the action to uniquely identify this line as
					# intended for full sha1 match
					echo "$sha1 +$action $fullsha"
				fi
			fi
		esac
	done >"$1.sq" <"$1"
	test -s "$1.sq" || return

	used=
	while read -r pick sha1 message
	do
		case " $used" in
		*" $sha1 "*) continue ;;
		esac
		printf '%s\n' "$pick $sha1 $message"
		used="$used$sha1 "
		while read -r squash action msg
		do
			case " $used" in
			*" $squash "*) continue ;;
			esac
			emit=0
			case "$action" in
			+*)
				action="${action#+}"
				# full sha1 prefix test
				case "$msg" in "$sha1"*) emit=1;; esac ;;
			*)
				# message prefix test
				case "$message" in "$msg"*) emit=1;; esac ;;
			esac
			if test $emit = 1; then
				printf '%s\n' "$action $squash $action! $msg"
				used="$used$squash "
			fi
		done <"$1.sq"
	done >"$1.rearranged" <"$1"
	cat "$1.rearranged" >"$1"
	rm -f "$1.sq" "$1.rearranged"
}

LF='
'
parse_onto () {
	case "$1" in
	*...*)
		if	left=${1%...*} right=${1#*...} &&
			onto=$(git merge-base --all ${left:-HEAD} ${right:-HEAD})
		then
			case "$onto" in
			?*"$LF"?* | '')
				exit 1 ;;
			esac
			echo "$onto"
			exit 0
		fi
	esac
	git rev-parse --verify "$1^0"
}

prepare_preserve_merges () {
	mkdir "$REWRITTEN" || die "Could not create directory $REWRITTEN"
	if test -z "$REBASE_ROOT"
	then
		for c in $(git merge-base --all $HEAD $UPSTREAM)
		do
			echo $ONTO > "$REWRITTEN"/$c ||
				die "Could not init rewritten commits"
		done
	fi

	# show merges
	MERGES_OPTION=--parents

	# Watch for commits that have been dropped by --cherry-pick
	# The idea is that all commits that are already in upstream
	# have a mapping $(cat "$REWRITTEN"/<my-sha1>) = <upstream-sha1>
	# as if they were rewritten.

	# Get all patch ids
	# --cherry-pick only analyzes first parent, -m analyzes _all_ parents!
	# So take only the first patch-id for each commit id (uniq -f1).
	git log -m -p $UPSTREAM..$HEAD | git patch-id |
		uniq -s 41 > "$REWRITTEN"/ours
	git log -m -p $HEAD..$UPSTREAM | git patch-id |
		uniq -s 41 > "$REWRITTEN"/upstream

	# Now get the correspondences
	cat "$REWRITTEN"/ours | while read patch_id commit
	do
		# Is the same patch id in the upstream?
		sane_grep "^$patch_id " < "$REWRITTEN"/upstream 2> /dev/null ||
		continue

		# Record the parent as "rewritten" commit.  As we will resolve
		# rewritten commits recursively, this will work even if the
		# parent was rewritten, too.
		#
		# If there is no parent, then we have a root commit that
		# was cherry-picked into upstream; let's use $ONTO as
		# fake parent of that root commit.
		upstream=$(git rev-parse --verify "$commit^" 2> /dev/null)
		test ! -z "$upstream" || upstream=$ONTO
		echo "$upstream" > "$REWRITTEN"/$commit
	done
}

get_oneline () {
	git show -s --format="%h %s" $1
}

generate_script_help () {
	cat << EOF

# Rebase $SHORTREVISIONS onto $SHORTONTO
#
# Commands:
#  p, pick = use commit
#  r, reword = use commit, but edit the commit message
#  e, edit = use commit, but stop for amending
#  s, squash = use commit, but meld into previous commit
#  f, fixup = like "squash", but discard this commit's log message
#  g, goto = reset the current state to the given commit
#  x, exec = run command (the rest of the line) using shell
#  m, merge parents <parents> original <original merge commit>
#          = redo the given merge commit
#
# If you remove a line here THAT COMMIT WILL BE LOST.
# However, if you remove everything, the rebase will be aborted.
#
EOF
}

# List the revs to go in $TODO, with additional rev-list args from "$@".
list_todo_revs () {
	git rev-list $MERGES_OPTION --cherry-pick \
		--topo-order --reverse $REVISIONS "$@"
}

generate_script () {
	test -z "$(list_todo_revs)" && {
		echo noop
		return
	}

	start=$SHORTUPSTREAM
	test -z "$REBASE_ROOT" || start=

	# Identify commits we will need to 'mark' in order to 'goto' back to.
	# We create empty files in $MARK_CONSTRUCT to be filled in on
	# the second pass.
	test -z "$PRESERVE_MERGES" || {
		mkdir -p "$MARK_CONSTRUCT"
		current=$start
		list_todo_revs --format="%m%h %p" |
		sed -n "s/^>//p" |
		while read shortsha1 firstparent rest
		do
			case "$firstparent" in
			$current*|$SHORTUPSTREAM*|'')
				;;
			*)
				touch "$MARK_CONSTRUCT"/$firstparent
				;;
			esac
			current=$shortsha1

			for parent in $rest
			do
				touch "$MARK_CONSTRUCT"/$parent
			done
		done
	}

	current=$start
	marknum=1
	list_todo_revs --format="%m%h %p" |
	sed -n "s/^>//p" |
	while read shortsha1 firstparent rest
	do
		count=$(($count+1))

		# generate "goto" statements
		test -z "$PRESERVE_MERGES" || {
			case "$firstparent" in
			$current*)
				# already there
				;;
			$SHORTUPSTREAM*|'')
				echo "goto $(get_oneline $SHORTONTO)"
				;;
			*)
				echo "goto $(cat "$MARK_CONSTRUCT"/$firstparent)"
				;;
			esac
			current=$shortsha1
		}

		if test -z "$rest"
		then
			echo "pick $(get_oneline $shortsha1)"
		else
			# handle merges
			parents=$(for p in $rest; do cat "$MARK_CONSTRUCT"/$p; done)
			echo "merge parents $parents original $(get_oneline $shortsha1)"
			for parent in $rest
			do
				echo "#    parent $(get_oneline $parent)"
			done
		fi

		if test -e "$MARK_CONSTRUCT"/$shortsha1
		then
			echo "mark :$marknum $shortsha1"
			echo ":$marknum" >"$MARK_CONSTRUCT"/$shortsha1
			marknum=$(expr $marknum + 1)
		fi
	done
}

while test $# != 0
do
	case "$1" in
	--no-verify)
		OK_TO_SKIP_PRE_REBASE=yes
		;;
	--verify)
		OK_TO_SKIP_PRE_REBASE=
		;;
	--continue)
		is_standalone "$@" || usage
		get_saved_options
		comment_for_reflog continue

		test -d "$DOTEST" || die "No interactive rebase running"

		# Sanity check
		git rev-parse --verify HEAD >/dev/null ||
			die "Cannot read HEAD"
		git update-index --ignore-submodules --refresh &&
			git diff-files --quiet --ignore-submodules ||
			die "Working tree is dirty"

		# do we have anything to commit?
		if git diff-index --cached --quiet --ignore-submodules HEAD --
		then
			: Nothing to commit
			add_rewritten
		else
			. "$AUTHOR_SCRIPT" ||
				die "Cannot find the author identity"
			amend=
			if test -f "$AMEND"
			then
				amend=$(git rev-parse --verify HEAD)
				test "$amend" = $(cat "$AMEND") ||
				die "\
You have uncommitted changes in your working tree. Please, commit them
first and then run 'git rebase --continue' again."
				git reset --soft HEAD^ ||
				die "Cannot rewind the HEAD"
			fi
			do_with_author git commit --no-verify -F "$MSG" -e &&
			add_rewritten || {
				test -n "$amend" && git reset --soft $amend
				die "Could not commit staged changes."
			}
		fi

		record_in_rewritten "$(cat "$DOTEST"/stopped-sha)"

		require_clean_work_tree "rebase"
		do_rest
		;;
	--abort)
		is_standalone "$@" || usage
		get_saved_options
		comment_for_reflog abort

		git rerere clear
		test -d "$DOTEST" || die "No interactive rebase running"

		HEADNAME=$(cat "$DOTEST"/head-name)
		HEAD=$(cat "$DOTEST"/head)
		case $HEADNAME in
		refs/*)
			git symbolic-ref HEAD $HEADNAME
			;;
		esac &&
		output git reset --hard $HEAD &&
		rm -rf "$DOTEST"
		exit
		;;
	--skip)
		is_standalone "$@" || usage
		get_saved_options
		comment_for_reflog skip

		git rerere clear
		test -d "$DOTEST" || die "No interactive rebase running"

		test -d "$REWRITTEN" && {
			# skip last to-be-rewritten commit
			original_count=$(wc -l < "$REWRITTEN"/original)
			test $original_count -gt 0 &&
			head -n $(($original_count-1)) < "$REWRITTEN"/original \
				> "$REWRITTEN"/original.new &&
			mv "$REWRITTEN"/original.new "$REWRITTEN"/original
		}
		output git reset --hard && do_rest
		;;
	-s)
		case "$#,$1" in
		*,*=*)
			STRATEGY="-s "$(expr "z$1" : 'z-[^=]*=\(.*\)') ;;
		1,*)
			usage ;;
		*)
			STRATEGY="-s $2"
			shift ;;
		esac
		;;
	-m)
		# we use merge anyway
		;;
	-v)
		VERBOSE=t
		;;
	-p)
		PRESERVE_MERGES=t
		;;
	-i)
		# yeah, we know
		;;
	--no-ff)
		NEVER_FF=t
		;;
	--root)
		REBASE_ROOT=t
		;;
	--autosquash)
		AUTOSQUASH=t
		;;
	--no-autosquash)
		AUTOSQUASH=
		;;
	--onto)
		shift
		ONTO=$(parse_onto "$1") ||
			die "Does not point to a valid commit: $1"
		;;
	--)
		shift
		test -z "$REBASE_ROOT" -a $# -ge 1 -a $# -le 2 ||
		test ! -z "$REBASE_ROOT" -a $# -le 1 || usage
		test -d "$DOTEST" &&
			die "Interactive rebase already started"

		git var GIT_COMMITTER_IDENT >/dev/null ||
			die "You need to set your committer info first"

		if test -z "$REBASE_ROOT"
		then
			UPSTREAM_ARG="$1"
			UPSTREAM=$(git rev-parse --verify "$1") ||
				die "Invalid base"
			test -z "$ONTO" && ONTO=$UPSTREAM
			shift
		else
			UPSTREAM=
			UPSTREAM_ARG=--root
			test -z "$ONTO" &&
				die "You must specify --onto when using --root"
			UPSTREAM=$ONTO
		fi
		run_pre_rebase_hook "$UPSTREAM_ARG" "$@"

		comment_for_reflog start

		require_clean_work_tree "rebase" "Please commit or stash them."

		if test ! -z "$1"
		then
			output git checkout "$1" -- ||
				die "Could not checkout $1"
		fi

		HEAD=$(git rev-parse --verify HEAD) || die "No HEAD?"
		mkdir "$DOTEST" || die "Could not create temporary $DOTEST"

		: > "$DOTEST"/interactive || die "Could not mark as interactive"
		git symbolic-ref HEAD > "$DOTEST"/head-name 2> /dev/null ||
			echo "detached HEAD" > "$DOTEST"/head-name

		echo $HEAD > "$DOTEST"/head
		case "$REBASE_ROOT" in
		'')
			rm -f "$DOTEST"/rebase-root ;;
		*)
			: >"$DOTEST"/rebase-root ;;
		esac
		echo $ONTO > "$DOTEST"/onto
		test -z "$STRATEGY" || echo "$STRATEGY" > "$DOTEST"/strategy
		test t = "$VERBOSE" && : > "$DOTEST"/verbose

		SHORTHEAD=$(git rev-parse --short $HEAD)
		SHORTONTO=$(git rev-parse --short $ONTO)
		SHORTUPSTREAM=$(git rev-parse --short $UPSTREAM)
		REVISIONS=$UPSTREAM...$HEAD
		SHORTREVISIONS=$SHORTUPSTREAM..$SHORTHEAD

		MERGES_OPTION=--no-merges
		test t = "$PRESERVE_MERGES" && prepare_preserve_merges

		generate_script > "$TODO"
		test -n "$AUTOSQUASH" && rearrange_squash "$TODO"
		generate_script_help >> "$TODO"

		has_action "$TODO" ||
			die_abort "Nothing to do"

		cp "$TODO" "$TODO".backup
		git_editor "$TODO" ||
			die_abort "Could not execute editor"

		has_action "$TODO" ||
			die_abort "Nothing to do"

		test -d "$REWRITTEN" || test -n "$NEVER_FF" || skip_unnecessary_picks

		output git checkout $ONTO || die_abort "could not detach HEAD"
		git update-ref ORIG_HEAD $HEAD
		do_rest
		;;
	esac
	shift
done
