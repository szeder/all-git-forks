#!/bin/sh
# Usage:
#	PATH=${git_src}/contrib/svn-fe:${git_src}/bin-wrappers:$PATH
#	testme.sh http://cvs2svn.tigris.org/svn/cvs2svn 10 test.git
set -e
: ${1?"URL?"}
: ${2?"How many revisions?"}
: ${3?"Git directory?"}

git init --bare "$3"
rm -f "$3/backflow"
mkfifo "$3/backflow"

svnrdump dump -r1:"$2" "$1" |
svn-fe 3<"$3/backflow" |
GIT_DIR=$3 git fast-import --cat-blob-fd=3 3>"$3/backflow"
