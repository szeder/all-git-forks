#!/bin/sh

test_description='check svn dumpfile importer'

. ./lib-git-svn.sh

test_dump() {
	label=$1
	dump=$2
	test_expect_success "$dump" '
		svnadmin create "$label-svn" &&
		svnadmin load "$label-svn" < "$TEST_DIRECTORY/$dump" &&
		svn_cmd export "file://$PWD/$label-svn" "$label-svnco" &&
		git init "$label-git" &&
		(
			cd "$label-git" && mkfifo backchannel && \
			test-svn-fe "$TEST_DIRECTORY/$dump" 3< backchannel | \
			git fast-import --cat-blob-fd=3 3> backchannel
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
