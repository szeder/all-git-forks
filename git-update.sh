#!/bin/sh
#
# Copyright (C) 2014 Felipe Contreras
# Copyright (C) 2005 Junio C Hamano
#
# Fetch upstream and update the current branch.

USAGE='[-n | --no-stat] [--[no-]commit] [--[no-]ff] [--[no-]rebase|--rebase=preserve] [-s strategy]...'
LONG_USAGE='Fetch upstram and update the current branch.'
SUBDIRECTORY_OK=Yes
OPTIONS_SPEC=
. git-sh-setup
. git-sh-i18n
set_reflog_action "update${1+ $*}"
require_work_tree_exists
cd_to_toplevel


warn () {
	printf >&2 'warning: %s\n' "$*"
}

die_conflict () {
	git diff-index --cached --name-status -r --ignore-submodules HEAD --
	die "$(gettext "Update is not possible because you have instaged files.")"
}

die_merge () {
	die "$(gettext "You have not concluded your merge (MERGE_HEAD exists).")"
}

test -z "$(git ls-files -u)" || die_conflict
test -f "$GIT_DIR/MERGE_HEAD" && die_merge

bool_or_string_config () {
	git config --bool "$1" 2>/dev/null || git config "$1"
}

strategy_args= diffstat= no_commit= no_ff= ff_only=
log_arg= verbosity= progress= recurse_submodules= verify_signatures=
merge_args= edit= rebase_args=
curr_branch=$(git symbolic-ref -q HEAD)
curr_branch_short="${curr_branch#refs/heads/}"
mode=$(git config branch.${curr_branch_short}.updatemode)
if test -z "$mode"
then
	mode=$(git config update.mode)
fi
case "$mode" in
merge|rebase|ff-only|'')
	;;
rebase-preserve)
	mode="rebase"
	rebase_args="--preserve-merges"
	;;
*)
	echo "Invalid value for 'mode'"
	usage
	exit 1
	;;
esac
# compatibility with pull configuration
if test -z "$mode"
then
	rebase=$(bool_or_string_config branch.$curr_branch_short.rebase)
	if test -z "$rebase"
	then
		rebase=$(bool_or_string_config pull.rebase)
	fi
fi
test -z "$mode" && mode=ff-only
dry_run=
while :
do
	case "$1" in
	-q|--quiet)
		verbosity="$verbosity -q" ;;
	-v|--verbose)
		verbosity="$verbosity -v" ;;
	--progress)
		progress=--progress ;;
	--no-progress)
		progress=--no-progress ;;
	-n|--no-stat|--no-summary)
		diffstat=--no-stat ;;
	--stat|--summary)
		diffstat=--stat ;;
	--log|--no-log)
		log_arg=$1 ;;
	-e|--edit)
		edit=--edit ;;
	--no-edit)
		edit=--no-edit ;;
	--no-commit)
		no_commit=--no-commit ;;
	--commit)
		no_commit=--commit ;;
	--ff)
		no_ff=--ff ;;
	--no-ff)
		no_ff=--no-ff ;;
	--ff-only)
		ff_only=--ff-only ;;
	-s=*|--strategy=*|-s|--strategy)
		case "$#,$1" in
		*,*=*)
			strategy=`expr "z$1" : 'z-[^=]*=\(.*\)'` ;;
		1,*)
			usage ;;
		*)
			strategy="$2"
			shift ;;
		esac
		strategy_args="${strategy_args}-s $strategy "
		;;
	-X*)
		case "$#,$1" in
		1,-X)
			usage ;;
		*,-X)
			xx="-X $(git rev-parse --sq-quote "$2")"
			shift ;;
		*,*)
			xx=$(git rev-parse --sq-quote "$1") ;;
		esac
		merge_args="$merge_args$xx "
		;;
	-r=*|--rebase=*)
		rebase="${1#*=}"
		;;
	-r|--rebase)
		mode=rebase
		;;
	-m|--merge)
		mode=merge
		;;
	--recurse-submodules)
		recurse_submodules=--recurse-submodules
		;;
	--recurse-submodules=*)
		recurse_submodules="$1"
		;;
	--no-recurse-submodules)
		recurse_submodules=--no-recurse-submodules
		;;
	--verify-signatures)
		verify_signatures=--verify-signatures
		;;
	--no-verify-signatures)
		verify_signatures=--no-verify-signatures
		;;
	--d|--dry-run)
		dry_run=--dry-run
		;;
	-h|--help-all)
		usage
		;;
	*)
		# Pass thru anything that may be meant for fetch.
		break
		;;
	esac
	shift
done

if test -n "$rebase"
then
	case "$rebase" in
	true)
		mode="rebase"
		;;
	false)
		mode="merge"
		;;
	preserve)
		mode="rebase"
		rebase_args=--preserve-merges
		;;
	*)
		echo "Invalid value for --rebase, should be true, false, or preserve"
		usage
		exit 1
		;;
	esac
fi

test -z "$curr_branch" &&
	die "$(gettext "You are not currently on a branch.")"

get_remote_branch () {
	local ref

	ref=$(git rev-parse --symbolic-full-name $1)

	git config --get-regexp 'remote.*.fetch' | while read key value
	do
		remote=${key#remote.}
		remote=${remote%.fetch}
		right=${value#*:}
		case $ref in
		$right)
			prefix=${right%\*}
			branch=${ref#$prefix}
			echo "remote=$remote branch=$branch"
			break
			;;
		esac
	done
}

case $# in
0)
	branch=$(git config "branch.$curr_branch_short.merge")
	remote=$(git config "branch.$curr_branch_short.remote")

	test -z "$branch" && branch=$curr_branch
	test -z "$remote" && remote="origin"
	;;
1)
	branch=$1
	remote=.

	eval $(get_remote_branch $1)
	;;
*)
	usage
	exit 1
	;;
esac

branch="${branch#refs/heads/}"

test "$mode" = rebase && {
	require_clean_work_tree "update with rebase" "Please commit or stash them."
	oldremoteref=$(git merge-base --fork-point $branch $curr_branch 2>/dev/null)
}
orig_head=$(git rev-parse -q --verify HEAD)

if test $remote = "."
then
	extra=--quiet
fi

git fetch $verbosity $progress $dry_run $recurse_submodules $remote $branch $extra || exit 1

test -z "$dry_run" || exit 0

merge_head=$(sed -e '/	not-for-merge	/d' -e 's/	.*//' "$GIT_DIR"/FETCH_HEAD)

test -z "$merge_head" &&
	die "$(gettext "Couldnot fetch branch '${branch#refs/heads/}'.")"

if test "$mode" = 'ff-only' && test -z "$no_ff$ff_only"
then
	# check if a non-fast-forward merge would be needed
	if ! git merge-base --is-ancestor "$orig_head" "$merge_head" &&
		! git merge-base --is-ancestor "$merge_head" "$orig_head"
	then
		die "$(gettext "The update was not fast-forward, please either merge or rebase.
If unsure, run 'git update --merge'.")"
	fi
	ff_only=--ff-only
fi

if test "$mode" = rebase
then
	o=$(git show-branch --merge-base $curr_branch $merge_head $oldremoteref)
	test "$oldremoteref" = "$o" && unset oldremoteref
fi

build_msg () {
	if test "$curr_branch_short" = "$branch"
	then
		echo "Update branch '$curr_branch_short'"
	else
		msg="Merge"
		msg="${msg} branch '$curr_branch_short'"
		test "$branch" != "master" && msg="${msg} into $branch"
		echo "$msg"
	fi
}

fix_msg () {
	build_msg
	sed -e '1 d'
}

case "$mode" in
rebase)
	eval="git-rebase $diffstat $strategy_args $merge_args $rebase_args $verbosity"
	eval="$eval --onto $merge_head ${oldremoteref:-$merge_head}"
	eval "exec $eval"
	;;
*)
	merge_msg=$(git fmt-merge-msg $log_arg < "$GIT_DIR"/FETCH_HEAD | fix_msg) || exit
	eval="git-merge $diffstat $no_commit $verify_signatures $edit $no_ff $ff_only"
	eval="$eval $log_arg $strategy_args $merge_args $verbosity $progress"
	eval="$eval --reverse-parents -m \"\$merge_msg\" $merge_head"
	eval "exec $eval"
	;;
esac
