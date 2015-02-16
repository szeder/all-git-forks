#!/bin/sh
#
# Manipulate the leading directories of tree objects.
#
# Copyright (c) 2015 Johan Herland

USAGE="[--extract <subdir>] [--insert <subdir>] <tree-ish>"
OPTIONS_SPEC=
. git-sh-setup

extract() {
	subdir="$1"
	tree="$2"

	case "$subdir" in
	/*)
		die_with_status 2 "Cannot extract subdir starting with /"
		;;
	*)
		git rev-parse --verify "$tree":"$subdir" ||
			die_with_status 3 "Failed to find $subdir within $tree"
		;;
	esac
}

insert() {
	subdir="$1"
	tree="$2"
	die "Prepending '$subdir' to '$tree' not yet implemented..."
}

extract_dir=
insert_dir=

while test $# != 0
do
	case "$1" in
	--extract*)
		case "$#,$1" in
		*,*=*)
			extract_dir=$(expr "z$1" : 'z-[^=]*=\(.*\)')
			;;
		1,*)
			usage ;;
		*)
			extract_dir="$2"
			shift ;;
		esac
		;;
	--insert*)
		case "$#,$1" in
		*,*=*)
			insert_dir=$(expr "z$1" : 'z-[^=]*=\(.*\)')
			;;
		1,*)
			usage ;;
		*)
			insert_dir="$2"
			shift ;;
		esac
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

test -n "$extract_dir" -o -n "$insert_dir" || usage
test $# = 1 || usage

treeish=$(git rev-parse --verify "$1")
tree=$(git rev-parse --verify "$treeish"^{tree})

if test -n "$extract_dir"
then
	tree=$(extract "$extract_dir" "$tree")
	ret="$?"
	test "$ret" = "0" || exit "$ret"
fi

if test -n "$insert_dir"
then
	tree=$(insert "$extract_dir" "$tree")
	ret="$?"
	test "$ret" = "0" || exit "$ret"
fi

echo "$tree"

exit 0
