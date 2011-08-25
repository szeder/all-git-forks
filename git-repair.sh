#!/bin/sh

USAGE='[color|grafts]'
LONG_USAGE='git repair
	color <commit> <color>
	grafts <rev-list-args>...

Please use "git help repair" to get the full man page.'

OPTIONS_SPEC=
. git-sh-setup
. git-sh-i18n

_x40='[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]'
_x40="$_x40$_x40$_x40$_x40$_x40$_x40$_x40$_x40"


GRAFTS_FILE=$(git rev-parse --git-dir)/info/grafts

info()
{
	echo "$*" 1>&2
}

green_commits()
{
	:
}

grafts()
{
	test -f "$GRAFTS_FILE" && cat "$GRAFTS_FILE"
}

graft()
{
	grafts | grep "^$1 .*\$"
}

repair_parents()
{
	info "repairing parents of $1..."
	separator=${2:- }
	set -- $(git rev-list --no-walk $1 --parents) || exit $?
	shift
	parents=''
	while test $# -gt 0
	do
		parents="$parents${separator}$(repair_commit $1)"
		shift
	done
	echo $parents
}

repair_commit()
{
	commit=$1
	sha1=$(git rev-parse --verify $commit) || die "'$commit' is not a commit"
	color=$(repair_color $commit | cut -f2 -d' ') || exit $?
	result=

	case "$color" in
	grey)
		result=$(git rev-parse --verify refs/replace/$sha1 2>/dev/null)
		if test -z "$result"
		then
			tree=$(git mktree </dev/null) &&
			parent_options=$(repair_parents $sha1 " -p ") &&
			result=$( (echo "replacement for damaged commit $sha1"; echo; git cat-file commit $sha1 | sed -n "1,/^\$/d;p") | git commit-tree $tree $parent_options) &&
			git update-ref refs/replace/$sha1 $result || die "failed to repair $sha1"
		fi
		info "rewrote $sha1 as $result"
	;;
	black)
		result=$(git rev-parse --verify refs/replace/$sha1 2>/dev/null)
		if test -z "$result"
		then
			tree=$(git mktree </dev/null) &&
			result=$(echo "replacement for missing commit $sha1" | git commit-tree $tree $parent_options) &&
			git update-ref refs/replace/$sha1 $result || die "failed to repair $sha1"
		fi
		info "rewrote $sha1 as $result"
	;;
	red)
		graft=$(graft "$sha1")
		if test -z "$graft"
		then
			parents=$(repair_parents $sha1)
			graft="$sha1 $parents"
			echo "$graft" >> "$GRAFTS_FILE"
		fi
		result=$sha1
		info "created graft $graft"
	;;
	*)
		echo $sha1
	;;
	esac
	echo $result
	return 0
}

repair_color()
{
	commit=$1
	color=''
	grafts=$(grafts)
	sha1=$(git rev-parse --verify "$commit") || die "'$commit' is not a valid commit."
	type=$(git cat-file -t $sha1) 2>/dev/null

	case "$type" in
		commit)
			:
		;;
		"")
			echo "black"
			return 0
		;;
		*)
			die "'$commit' is not a commit"
		;;
	esac


	if 	git rev-list $sha1 --objects --not $(green_commits) > /tmp/$$.color 2>/dev/null &&
		git pack-objects --stdout < /tmp/$$.color >/dev/null 2>/dev/null
	then
		if git rev-list $sha1 --not $(grafts) --boundary | grep "^-" >/dev/null
		then
			output="orange"
		elif test -n "$grafts" && git rev-list --no-walk $grafts | grep "^$sha1" >/dev/null
		then
			output="red"
		else
			output="green"
		fi
	elif	git rev-list --no-walk $commit --objects > /tmp/$$.color 2>/dev/null &&
		git pack-objects --stdout < /tmp/$$.color >/dev/null 2>/dev/null
	then
		if	git rev-list --no-walk $(git rev-list --no-walk $commit --parents) --objects >/tmp/$$.color 2>/dev/null &&
			git pack-objects --stdout < /tmp/$$.color >/dev/null 2>/dev/null
		then
			output="orange"
		else
			output="red"
		fi
	else
		output="grey"
	fi
	rm -f /tmp/$$.color
	echo "$sha1 $output"
}

repair_grafts()
{
	:
}


case "$#" in
0)
	usage ;;
*)
	cmd="$1"
	shift
	case "$cmd" in
	color)
		repair_color "$@"
		;;
	grafts)
		repair_grafts "$@"
		;;
	commit)
		repair_commit "$@"
		;;
	*)
		usage ;;
	esac
esac
