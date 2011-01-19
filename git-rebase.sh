#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano.
#

USAGE='[--interactive | -i] [-v] [--force-rebase | -f] [--no-ff] [--onto <newbase>] (<upstream>|--root) [<branch>] [--quiet | -q]'
LONG_USAGE='git-rebase replaces <branch> with a new branch of the
same name.  When the --onto option is provided the new branch starts
out with a HEAD equal to <newbase>, otherwise it is equal to <upstream>
It then attempts to create a new commit for each commit from the original
<branch> that does not exist in the <upstream> branch.

It is possible that a merge failure will prevent this process from being
completely automatic.  You will have to resolve any such merge failure
and run git rebase --continue.  Another option is to bypass the commit
that caused the merge failure with git rebase --skip.  To restore the
original <branch> and remove the .git/rebase-apply working files, use the
command git rebase --abort instead.

Note that if <branch> is not specified on the command line, the
currently checked out branch is used.

Example:       git-rebase master~1 topic

        A---B---C topic                   A'\''--B'\''--C'\'' topic
       /                   -->           /
  D---E---F---G master          D---E---F---G master
'

SUBDIRECTORY_OK=Yes
OPTIONS_SPEC=
. git-sh-setup
set_reflog_action rebase
require_work_tree
cd_to_toplevel

LF='
'
OK_TO_SKIP_PRE_REBASE=
RESOLVEMSG="
When you have resolved this problem run \"git rebase --continue\".
If you would prefer to skip this patch, instead run \"git rebase --skip\".
To restore the original branch and stop rebasing run \"git rebase --abort\".
"
unset onto
strategy=
strategy_opts=
do_merge=
merge_dir="$GIT_DIR"/rebase-merge
apply_dir="$GIT_DIR"/rebase-apply
verbose=
diffstat=
test "$(git config --bool rebase.stat)" = true && diffstat=t
git_am_opt=
rebase_root=
force_rebase=
allow_rerere_autoupdate=
# Non-empty if a rebase was in progress when 'git rebase' was invoked
in_progress=
# One of {am, merge, interactive}
type=
# One of {"$GIT_DIR"/rebase-apply, "$GIT_DIR"/rebase-merge}
state_dir=
# One of {'', continue, skip, abort}, as parsed from command line
action=
preserve_merges=
autosquash=
test "$(git config --bool rebase.autosquash)" = "true" && autosquash=t

read_basic_state () {
	head_name=$(cat "$state_dir"/head-name) &&
	onto=$(cat "$state_dir"/onto) &&
	if test "$type" = interactive
	then
		orig_head=$(cat "$state_dir"/head)
	else
		orig_head=$(cat "$state_dir"/orig-head)
	fi &&
	GIT_QUIET=$(cat "$state_dir"/quiet) &&
	test -f "$state_dir"/verbose && verbose=t
	test -f "$state_dir"/strategy && strategy="$(cat "$state_dir"/strategy)"
	test -f "$state_dir"/strategy_opts &&
		strategy_opts="$(cat "$state_dir"/strategy_opts)"
	test -f "$state_dir"/allow_rerere_autoupdate &&
		allow_rerere_autoupdate="$(cat "$state_dir"/allow_rerere_autoupdate)"
}

write_basic_state () {
	echo "$head_name" > "$state_dir"/head-name &&
	echo "$onto" > "$state_dir"/onto &&
	if test "$type" = interactive
	then
		echo "$orig_head" > "$state_dir"/head
	else
		echo "$orig_head" > "$state_dir"/orig-head
	fi &&
	echo "$GIT_QUIET" > "$state_dir"/quiet &&
	test t = "$verbose" && : > "$state_dir"/verbose
	test -n "$strategy" && echo "$strategy" > "$state_dir"/strategy
	test -n "$strategy_opts" && echo "$strategy_opts" > \
		"$state_dir"/strategy_opts
	test -n "$allow_rerere_autoupdate" && echo "$allow_rerere_autoupdate" > \
		"$state_dir"/allow_rerere_autoupdate
}

output () {
	case "$verbose" in
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

move_to_original_branch () {
	case "$head_name" in
	refs/*)
		message="rebase finished: $head_name onto $onto"
		git update-ref -m "$message" \
			$head_name $(git rev-parse HEAD) $orig_head &&
		git symbolic-ref HEAD $head_name ||
		die "Could not move back to $head_name"
		;;
	esac
}

run_specific_rebase () {
	if [ "$interactive_rebase" = implied ]; then
		GIT_EDITOR=:
		export GIT_EDITOR
	fi
	export onto autosquash strategy strategy_opts verbose rebase_root \
	force_rebase action preserve_merges upstream switch_to head_name \
	state_dir orig_head onto_name GIT_QUIET revisions RESOLVEMSG \
	allow_rerere_autoupdate git_am_opt type
	export -f move_to_original_branch output write_basic_state
	exec git-rebase--$type
}

run_pre_rebase_hook () {
	if test -z "$OK_TO_SKIP_PRE_REBASE" &&
	   test -x "$GIT_DIR/hooks/pre-rebase"
	then
		"$GIT_DIR/hooks/pre-rebase" ${1+"$@"} ||
		die "The pre-rebase hook refused to rebase."
	fi
}

test -f "$apply_dir"/applying &&
	die 'It looks like git-am is in progress. Cannot rebase.'

if test -d "$apply_dir"
then
	type=am
	state_dir="$apply_dir"
elif test -d "$merge_dir"
then
	if test -f "$merge_dir"/interactive
	then
		type=interactive
		interactive_rebase=explicit
	else
		type=merge
	fi
	state_dir="$merge_dir"
fi
test -n "$type" && in_progress=t

total_argc=$#
while test $# != 0
do
	case "$1" in
	--no-verify)
		OK_TO_SKIP_PRE_REBASE=yes
		;;
	--verify)
		OK_TO_SKIP_PRE_REBASE=
		;;
	--continue|--skip|--abort)
		test $total_argc -eq 1 || usage
		action=${1##--}
		;;
	--onto)
		test 2 -le "$#" || usage
		onto="$2"
		shift
		;;
	-i|--interactive)
		interactive_rebase=explicit
		;;
	-p|--preserve-merges)
		preserve_merges=t
		test -z "$interactive_rebase" && interactive_rebase=implied
		;;
	--autosquash)
		autosquash=t
		;;
	--no-autosquash)
		autosquash=
		;;
	-M|-m|--m|--me|--mer|--merg|--merge)
		do_merge=t
		;;
	-X*|--strategy-option*)
		case "$#,$1" in
		1,-X|1,--strategy-option)
			usage ;;
		*,-X|*,--strategy-option)
			newopt="$2"
			shift ;;
		*,--strategy-option=*)
			newopt="$(expr " $1" : ' --strategy-option=\(.*\)')" ;;
		*,-X*)
			newopt="$(expr " $1" : ' -X\(.*\)')" ;;
		1,*)
			usage ;;
		esac
		strategy_opts="$strategy_opts $(git rev-parse --sq-quote "--$newopt")"
		do_merge=t
		test -z "$strategy" && strategy=recursive
		;;
	-s=*|--s=*|--st=*|--str=*|--stra=*|--strat=*|--strate=*|\
		--strateg=*|--strategy=*|\
	-s|--s|--st|--str|--stra|--strat|--strate|--strateg|--strategy)
		case "$#,$1" in
		*,*=*)
			strategy=`expr "z$1" : 'z-[^=]*=\(.*\)'` ;;
		1,*)
			usage ;;
		*)
			strategy="$2"
			shift ;;
		esac
		do_merge=t
		;;
	-n|--no-stat)
		diffstat=
		;;
	--stat)
		diffstat=t
		;;
	-v|--verbose)
		verbose=t
		diffstat=t
		GIT_QUIET=
		;;
	-q|--quiet)
		GIT_QUIET=t
		git_am_opt="$git_am_opt -q"
		verbose=
		diffstat=
		;;
	--whitespace=*)
		git_am_opt="$git_am_opt $1"
		case "$1" in
		--whitespace=fix|--whitespace=strip)
			force_rebase=t
			;;
		esac
		;;
	--ignore-whitespace)
		git_am_opt="$git_am_opt $1"
		;;
	--committer-date-is-author-date|--ignore-date)
		git_am_opt="$git_am_opt $1"
		force_rebase=t
		;;
	-C*)
		git_am_opt="$git_am_opt $1"
		;;
	--root)
		rebase_root=t
		;;
	-f|--f|--fo|--for|--forc|--force|--force-r|--force-re|--force-reb|--force-reba|--force-rebas|--force-rebase|--no-ff)
		force_rebase=t
		;;
	--rerere-autoupdate|--no-rerere-autoupdate)
		allow_rerere_autoupdate="$1"
		;;
	-*)
		usage
		;;
	*)
		break
		;;
	esac
	shift
done
test $# -gt 2 && usage

if test -n "$action"
then
	test -z "$in_progress" && die "No rebase in progress?"
	# Only interactive rebase uses detailed reflog messages
	if test "$type" = interactive && test "$GIT_REFLOG_ACTION" = rebase
	then
		GIT_REFLOG_ACTION="rebase -i ($action)"
		export GIT_REFLOG_ACTION
	fi
fi

case "$action" in
continue)
	# Sanity check
	git rev-parse --verify HEAD >/dev/null ||
		die "Cannot read HEAD"
	git update-index --ignore-submodules --refresh &&
	git diff-files --quiet --ignore-submodules || {
		echo "You must edit all merge conflicts and then"
		echo "mark them as resolved using git add"
		exit 1
	}
	read_basic_state
	run_specific_rebase
	;;
skip)
	output git reset --hard HEAD || exit $?
	read_basic_state
	run_specific_rebase
	;;
abort)
	git rerere clear
	read_basic_state
	case "$head_name" in
	refs/*)
		git symbolic-ref HEAD $head_name ||
		die "Could not move back to $head_name"
		;;
	esac
	output git reset --hard $orig_head
	rm -r "$state_dir"
	exit
	;;
esac

# Make sure no rebase is in progress
if test -n "$in_progress"
then
	die '
It seems that there is already a '"${state_dir##*/}"' directory, and
I wonder if you are in the middle of another rebase.  If that is the
case, please try
	git rebase (--continue | --abort | --skip)
If that is not the case, please
	rm -fr '"$state_dir"'
and run me again.  I am stopping in case you still have something
valuable there.'
fi

test $# -eq 0 && test -z "$rebase_root" && usage

if test -n "$interactive_rebase"
then
	type=interactive
	state_dir="$merge_dir"
elif test -n "$do_merge"
then
	type=merge
	state_dir="$merge_dir"
else
	type=am
	state_dir="$apply_dir"
fi

if test -z "$rebase_root"
then
	# The upstream head must be given.  Make sure it is valid.
	upstream_name="$1"
	shift
	upstream=`git rev-parse --verify "${upstream_name}^0"` ||
	die "invalid upstream $upstream_name"
	upstream_arg="$upstream_name"
else
	test -z "$onto" && die "You must specify --onto when using --root"
	unset upstream_name
	unset upstream
	upstream_arg=--root
fi

# Make sure the branch to rebase onto is valid.
onto_name=${onto-"$upstream_name"}
case "$onto_name" in
*...*)
	if	left=${onto_name%...*} right=${onto_name#*...} &&
		onto=$(git merge-base --all ${left:-HEAD} ${right:-HEAD})
	then
		case "$onto" in
		?*"$LF"?*)
			die "$onto_name: there are more than one merge bases"
			;;
		'')
			die "$onto_name: there is no merge base"
			;;
		esac
	else
		die "$onto_name: there is no merge base"
	fi
	;;
*)
	onto=$(git rev-parse --verify "${onto_name}^0") ||
	die "Does not point to a valid commit: $1"
	;;
esac

# If the branch to rebase is given, that is the branch we will rebase
# $branch_name -- branch being rebased, or HEAD (already detached)
# $orig_head -- commit object name of tip of the branch before rebasing
# $head_name -- refs/heads/<that-branch> or "detached HEAD"
switch_to=
case "$#" in
1)
	# Is it "rebase other $branchname" or "rebase other $commit"?
	branch_name="$1"
	switch_to="$1"

	if git show-ref --verify --quiet -- "refs/heads/$1" &&
	   orig_head=$(git rev-parse -q --verify "refs/heads/$1")
	then
		head_name="refs/heads/$1"
	elif orig_head=$(git rev-parse -q --verify "$1")
	then
		head_name="detached HEAD"
	else
		usage
	fi
	;;
*)
	# Do not need to switch branches, we are already on it.
	if branch_name=`git symbolic-ref -q HEAD`
	then
		head_name=$branch_name
		branch_name=`expr "z$branch_name" : 'zrefs/heads/\(.*\)'`
	else
		head_name="detached HEAD"
		branch_name=HEAD ;# detached
	fi
	orig_head=$(git rev-parse --verify "${branch_name}^0") || exit
	;;
esac

require_clean_work_tree "rebase" "Please commit or stash them."

# Now we are rebasing commits $upstream..$orig_head (or with --root,
# everything leading up to $orig_head) on top of $onto

# Check if we are already based on $onto with linear history,
# but this should be done only when upstream and onto are the same
# and if this is not an interactive rebase.
mb=$(git merge-base "$onto" "$orig_head")
if test "$type" != interactive && test "$upstream" = "$onto" &&
	test "$mb" = "$onto" &&
	# linear history?
	! (git rev-list --parents "$onto".."$orig_head" | sane_grep " .* ") > /dev/null
then
	if test -z "$force_rebase"
	then
		# Lazily switch to the target branch if needed...
		test -z "$switch_to" || git checkout "$switch_to"
		say "Current branch $branch_name is up to date."
		exit 0
	else
		say "Current branch $branch_name is up to date, rebase forced."
	fi
fi

# If a hook exists, give it a chance to interrupt
run_pre_rebase_hook "$upstream_arg" "$@"

if test -n "$diffstat"
then
	if test -n "$verbose"
	then
		echo "Changes from $mb to $onto:"
	fi
	# We want color (if set), but no pager
	GIT_PAGER='' git diff --stat --summary "$mb" "$onto"
fi

test "$type" = interactive && run_specific_rebase

# Detach HEAD and reset the tree
say "First, rewinding head to replay your work on top of it..."
git checkout -q "$onto^0" || die "could not detach HEAD"
git update-ref ORIG_HEAD $orig_head

# If the $onto is a proper descendant of the tip of the branch, then
# we just fast-forwarded.
if test "$mb" = "$orig_head"
then
	say "Fast-forwarded $branch_name to $onto_name."
	move_to_original_branch
	exit 0
fi

if test -n "$rebase_root"
then
	revisions="$onto..$orig_head"
else
	revisions="$upstream..$orig_head"
fi

run_specific_rebase
