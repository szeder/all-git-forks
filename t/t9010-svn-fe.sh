#!/bin/sh

test_description='check svn dumpfile importer'

. ./test-lib.sh

svnfe_bin=$GIT_EXEC_PATH/contrib/svn-fe/svn-fe
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
	test_expect_success SVNFE "$dump" '
		svnadmin create "$label-svn" &&
		svnadmin load "$label-svn" < "$TEST_DIRECTORY/$dump" &&
		svn_cmd export "file://$PWD/$label-svn" "$label-svnco" &&
		git init "$label-git" &&
		$svnfe_bin <"$TEST_DIRECTORY/$dump" >"$label.fe" &&
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

if [ -x $svnfe_bin ]; then
    test_set_prereq SVNFE
fi

test_dump simple t9135/svn.dump

test_done
