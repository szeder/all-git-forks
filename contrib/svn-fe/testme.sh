#!/bin/bash
# Usage:
#	PATH=${git_src}/contrib/svn-fe:${git_src}/bin-wrappers:$PATH
#	testme.sh http://cvs2svn.tigris.org/svn/cvs2svn 0:10 test.git
set -e
: ${1?"URL?"}
: ${2?"Which revisions?"}
: ${3?"Git directory?"}

if test "${4+set}"
then
	importmarks=--import-marks=svnrev
	mkdir "$3/old+"
	mv "$3/"*.xz "$3/old+/"
	if test -e "$3/old"
	then
		mv "$3/old" "$3/old+/"
	fi
	mv "$3/old+" "$3/old"
else
	importmarks=
	git init --bare "$3"
fi
rm -f "$3/backflow"
mkfifo "$3/backflow"

svnrdump dump -r"$2" "$1" |
tee >(xz -1 >"$3/dump.xz") |
svn-fe 3<"$3/backflow" |
tee >(xz -1 >"$3/stream.xz") |
GIT_DIR=$3 git fast-import --cat-blob-fd=3 \
		--relative-marks $importmarks --export-marks=svnrev 3>&1 >&2 |
tee >(xz -1 >"$3/blobs.xz") >"$3/backflow"
