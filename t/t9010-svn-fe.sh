#!/bin/sh

test_description='check svn dumpfile importer'

. ./test-lib.sh

svnfe_bin=$GIT_EXEC_PATH/contrib/svn-fe/svn-fe
parser_bin=$GIT_EXEC_PATH/contrib/svn-fe/svndiff-parser
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

test_parse() {
	diff0=$1
	source=$2
	expected=$3
	test_expect_success PARSER "$diff0" '
		$parser_bin "$TEST_DIRECTORY/$source" <"$TEST_DIRECTORY/$diff0" >actual &&
		test_cmp "$TEST_DIRECTORY/$expected" actual
	'
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

if [ -x $svndiff_parser ]; then
    test_set_prereq PARSER
fi

test_dump simple t9135/svn.dump
test_parse t9135/newdata.diff0 t9135/blank.done t9135/newdata.done
test_parse t9135/src.diff0 t9135/newdata.done t9135/src.done

test_done
