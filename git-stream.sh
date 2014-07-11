#!/bin/sh
# Copyright (c) 2011, Geert Bosch

dashless=$(basename "$0" | sed -e 's/-/ /')
USAGE="import [<options>][<file> ...]
   or: $dashless export <stream>
   or: $dashless verify <stream>

    -n, --no-write        do not actually generate pack
    -v, --verbose         be more verbose
    -b, --bson		  split input on bson document boundaries
"
SUBDIRECTORY_OK=Yes
OPTIONS_SPEC=
. git-sh-setup

TMP="$GIT_DIR/.git-stream.$$"
trap 'rm -f "$TMP-*"' 0

import_stream() {
	opt_no_write=
	opt_verbose=
	opt_full=
	opt_bson=
	while test $# != 0
	do
		case $1 in
		-b|--bson)
			opt_bson=-b
			;;
		-n|--no-write)
			opt_no_write=-n
			;;
		-v|--verbose)
			opt_verbose=-v
			;;
		--)
			shift
			break
			;;
		-*)
			echo "error: unknown option for 'stream import': $1"
			usage
			;;
		*)
			break
			;;
		esac
		shift
	done

	tree=$(git-split $opt_bson $opt_no_write $opt_full $opt_verbose "$@") ||
		die "Cannot import stream"

	if test -z "$opt_no_write"
	then
		for pack in split_tmp*
		do
			test -f "$pack" &&
			git index-pack --stdin --keep --strict <"$pack" |
				(read type name && test "$type" = "keep" &&
				rm -f ".git/objects/pack/pack-$name.$type") &&
			rm -f "$pack"
		done
	fi
	for t in $tree
	do
		if test -z "$opt_no_write" -a $# -ne 0
		then
			tag=streams/$(basename "$1" .bson)
			echo "$tag" "$t"
			git tag -f "$tag" "$t"
		else
			echo "$t"
		fi
	done
}

export_stream() {
	if test $# -eq 0
	then
		echo "error: missing <stream> for 'stream export'"
		usage
	fi
	while test $# != 0
	do
		git ls-tree -r $1|cut -c13-52|xargs git show
		shift
	done
}

verify_stream() {
	if test $# -eq 0
	then
		echo "error: missing <stream> for 'stream verify'"
		usage
	fi
	while test $# != 0
	do
		trees=$(echo $1 ; git ls-tree -dr $1 | cut -c13-52)
		for t in $trees
		do
			if test $(git cat-file tree $t | wc -c) -gt 16384 ; then
				die "tree too large: $t"
			fi
		done
		git ls-tree -lr $1 | awk '
		{
			gsub("/", " ")
			prev_ent_ofs = ent_ofs
			ent_ofs=int($NF)
			tree_ofs=0
			for (j=5; j<NF;j++) tree_ofs += $j
			if (ent_ofs==prev_ent_ofs && tree_ofs>0) {
				 print "invalid break:" $0
			}
			offset = tree_ofs + ent_ofs
			if (sum != offset) {
				 print sum,"!=",offset,":" $0
			}
			sum += $4
		}
		END { print sum }'
		shift
	done
}

case "$1" in
import)
	shift
	import_stream "$@"
	;;
export)
	shift
	export_stream "$@"
	;;
verify)
	shift
	verify_stream "$@"
	;;
*)
	usage
	;;
esac

