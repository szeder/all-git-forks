#!/bin/sh
[[ -n $LX ]] && set -x
#
# Copyright (C) 2012 Robin Rosenberg
# Helper for adding symbolic links when file system does not support it
#

SUBDIRECTORY_OK=Yes
OPTIONS_KEEPDASHDASH=
OPTIONS_SPEC="\
git ln -s [options] target linkname
--
f,force	force creating when linkname or target does not exist
s,symbolic create symbolic link (mandatory)
cached operate only on index
"
#usage() {
	#	echo >&2 "usage: ln [-fs] target name"
	#	exit 1
#}

symbolic=
force=
cached=
copy=
symlinks=$(git config --get core.symlinks|tr A-Z a-z)
if [[ $symlinks == copy ]];then copy=1;fi

. git-sh-setup
. git-sh-i18n

require_work_tree_exists
if [ $? != 0 ]; then
	usage
fi
while test $# != 0;do
	case "$1" in
		-f|--force)
			force=1
			shift
			;;
		-s|--symbolic)
			symbolic=1
			shift
			;;
		--cached)
			cached=1
			shift
			;;
		--)
			if [[ $# == 3 ]];
			then
				target=$2
				name=$3
				shift 3
				break
			else
				usage
				exit 1
			fi
			;;
		*)
			usage
			exit 1
			;;
		esac
done
if [[ -z $symbolic ]];then
	usage
fi
GIT_DIR=$(git rev-parse --git-dir) || exit 1
cwd=$(pwd)
wd=$( (cd_to_toplevel && pwd) )
namedir=$(dirname "$name")
namebase=$(basename "$name")
absnamedir=$( (cd "$namedir" && pwd) )
cd "$namedir" || exit 1
pwd
if [[ ${absnamedir:0:${#wd}} != $wd ]]
then
	echo >&2 git-ln: $name is outside of working tree
	exit 1
fi
reldir=${absnamedir:${#wd}}
reldir=${reldir:1}
if [[ -n $reldir ]];then
	reldir="$reldir/"
fi
if [[ ! -e $target && ! $force ]];then
	echo >&2 git-ln: $target does not exist
	exit 1
fi

if [[ -n $force ]];then
	if [[ -z $cached ]];then
		if [[ -d $name && -n $copy ]];then
			rm -rf "$name"
		elif [[ ! -d $name ]];then
			rm -f "$name"
		fi
	fi
	git rm -f --cached "$name" >/dev/null || exit 1
fi
sha1=$(printf %s "$target" | git hash-object -w -t blob --stdin)
if [ $? != 0 ];then exit 1;fi
git update-index --add --cacheinfo 120000 $sha1 $reldir$name || exit $?
if [[ -n $copy ]];then
	cd "$cwd" || exit $?
	if [[ -d $target ]];
	then
		(cd "$target" &&
		find . -type d)|
		while read d;do
			mkdir -p "$name/$d" || exit $?
		done
		(cd "$target" &&
		find . -type f)|
		while read f;do
			ln "$target/$f" "$name/$f" || exit $?
		done
	else
		ln "$target" "$name" || exit $?
	fi
else
	if [[ -e $name ]];then
		echo >&2 git-ln: File $name exists
		exit 1
	fi
	cd "$cwd" || exit $?
	ln -s $target $name || exit $?
fi
exit 0
