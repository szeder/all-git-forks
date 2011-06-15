#!/bin/sh
set -e

die () {
       printf >&2 'fatal: %s\n' "$*"
       exit 128
}

usage () {
       printf >&2 'usage: %s\n' "$*"
       exit 129
}

do_fetch () {
       revs=$1 url=$2 tempdir=$3

       mkfifo "$tempdir/dumpfile"      # svnrdump output
       mkfifo "$tempdir/stream"        # svn-fe output
       mkfifo "$tempdir/backflow"      # responses to svn-fe queries

       svnrdump dump --non-interactive --username=Guest --password= \
               -r"$revs" "$url" >"$tempdir/dumpfile" &
       rdump_pid=$!

       svn-fe <"$tempdir/dumpfile" >"$tempdir/stream" 3<"$tempdir/backflow" &
       svnfe_pid=$!

       git fast-import --relative-marks --export-marks=svnrev \
               --cat-blob-fd=3 <"$tempdir/stream" >&2 3>"$tempdir/backflow" &
       gitfi_pid=$!

       wait "$rdump_pid" || die "remote-svn: svnrdump failed: $?"
       wait "$svnfe_pid" || die "remote-svn: svn-fe failed: $?"
       wait "$gitfi_pid" || die "remote-svn: git fast-import failed: $?"
}

do_import () {
       revs=$1 url=$2
       (svnrdump dump --non-interactive --username=Guest --password= \
               -r"$revs" "$url" | svn-fe) 3<&0 || die "FAILURE"
}

test "${2+set}" ||
usage 'git remote-svn <repository> <URL> < commandlist'
url=$2
_z8=00000000
_z40=$_z8$_z8$_z8$_z8$_z8

# NEEDSWORK: set up pipes in memory
tempdir=
trap 'rm -rf "$tempdir" || die "remote-svn: cannot remove temp dir"' EXIT
tempdir=$(mktemp --tmpdir -d remote-svn.XXXXXXXXX)

while read -r cmd args
do
       case $cmd in
       capabilities)
               echo import
               echo
               ;;
       list)
               echo '? refs/heads/master'
               echo
               ;;
       fetch)
               test "$args" = "$_z40 refs/heads/master" ||
               die "remote-svn: unsupported fetch arguments: $args"

               do_fetch 0:HEAD "$url" "$tempdir"
               ;;
       import)
               test "$args" = "refs/heads/master" ||
               die "remote-svn: unsupported import ref argument: $args"

               do_import 0:HEAD "$url"
               ;;
       '')
               echo
               ;;
       exit)
               exit 0
               ;;
       *)
               die "remote-svn: unsupported command: $cmd $args"
       esac
done
