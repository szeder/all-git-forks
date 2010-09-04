#!/bin/sh

test_description='check svn dumpfile importer'

. ./test-lib.sh

svnconf=$PWD/svnconf
export svnconf

svn_cmd () {
	[ -d "$svnconf" ] || mkdir "$svnconf"
	orig_svncmd="$1"; shift
	if [ -z "$orig_svncmd" ]; then
		svn
		return
	fi
	svn "$orig_svncmd" --config-dir "$svnconf" "$@"
}

test_dump() {
	label=$1
	dump=$2
	test_expect_success "$dump" '
		svnadmin create "$label-svn" &&
		svnadmin load "$label-svn" < "$TEST_DIRECTORY/$dump" &&
		svn_cmd export "file://$PWD/$label-svn" "$label-svnco" &&
		git init "$label-git" &&
		test-svn-fe "$TEST_DIRECTORY/$dump" >"$label.fe" &&
		(
			cd "$label-git" &&
			git fast-import < ../"$label.fe"
		) &&
		(
			cd "$label-svnco" &&
			git init &&
			git add . &&
			git fetch "../$label-git" master &&
			git diff --exit-code FETCH_HEAD
		)
	'
}

test_dump simple t9135/svn.dump

test_done
