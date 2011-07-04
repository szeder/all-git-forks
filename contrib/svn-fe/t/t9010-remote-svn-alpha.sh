#!/bin/sh

test_description='check svn-alpha remote helper'

PATH=$(pwd)/..:$PATH
TEST_DIRECTORY=$(pwd)/../../../t
. $TEST_DIRECTORY/test-lib.sh

if command -v svnrdump >/dev/null; then
	test_set_prereq SVNRDUMP
fi

deinit_git () {
	rm -fr .git
}

reinit_git () {
	deinit_git &&
	git init
}

properties () {
	while test "$#" -ne 0
	do
		property="$1" &&
		value="$2" &&
		printf "%s\n" "K ${#property}" &&
		printf "%s\n" "$property" &&
		printf "%s\n" "V ${#value}" &&
		printf "%s\n" "$value" &&
		shift 2 ||
		return 1
	done
}

text_no_props () {
	text="$1
" &&
	printf "%s\n" "Prop-content-length: 10" &&
	printf "%s\n" "Text-content-length: ${#text}" &&
	printf "%s\n" "Content-length: $((${#text} + 10))" &&
	printf "%s\n" "" "PROPS-END" &&
	printf "%s\n" "$text"
}

dump_to_svnrepo () {
	dump="$1" &&
	path="$2" &&
	svnadmin create "$path" &&
	svnadmin load "$path" < "$dump"
}

svnurl () {
	printf "svn-alpha::file://%s/%s" "$(pwd)" "$1"
}

test_nr_revs () {
	n=$1 &&
	repo=$2 &&
	ref=$3 &&
	git --git-dir="$repo" log --format=oneline "$ref" >revs &&
	test_line_count = "$n" revs
}

test_expect_success 'svnadmin is present' '
	command -v svnadmin &&
	test_set_prereq SVNADMIN
'

test_expect_success SVNADMIN 'create empty svnrepo' '
	echo "SVN-fs-dump-format-version: 2" > empty.dump &&
	dump_to_svnrepo empty.dump empty.svn &&
	test_set_prereq EMPTY_SVN
'

test_expect_success SVNADMIN 'create tiny svnrepo' '
	{
		properties \
			svn:author author@example.com \
			svn:date "1999-02-01T00:01:002.000000Z" \
			svn:log "add directory with some files in it" &&
		echo PROPS-END
	} >props &&
	{
		cat <<-EOF &&
		SVN-fs-dump-format-version: 3

		Revision-number: 1
		EOF
		echo Prop-content-length: $(wc -c <props) &&
		echo Content-length: $(wc -c <props) &&
		echo &&
		cat props &&
		cat <<-\EOF &&

		Node-path: directory
		Node-kind: dir
		Node-action: add
		Prop-content-length: 10
		Content-length: 10

		PROPS-END
		Node-path: directory/somefile
		Node-kind: file
		Node-action: add
		EOF
		text_no_props hi
	} >tiny.dump &&
	dump_to_svnrepo tiny.dump tiny.svn &&
	test_set_prereq TINY_SVN
'

test_expect_success SVNADMIN 'create small svndump' '
	{
		properties \
			svn:author author@example.com \
			svn:date "1999-02-01T00:01:002.000000Z" \
			svn:log "add directory with some files in it" &&
		echo PROPS-END
	} >props &&
	{
		echo Prop-content-length: $(wc -c <props) &&
		echo Content-length: $(wc -c <props) &&
		echo &&
		cat props
	} >props_dump &&
	cat >small.dump.r0 <<-EOF &&
	SVN-fs-dump-format-version: 3
	EOF
	for x in `seq 1 1 10`; do
		{
			echo &&
			echo "Revision-number: $x" &&
			cat props_dump &&

			if test "$x" -eq "1"; then
				cat <<-\EOF

				Node-path: directory
				Node-kind: dir
				Node-action: add
				Prop-content-length: 10
				Content-length: 10

				PROPS-END

				EOF
			fi
			echo "Node-path: directory/somefile$x" &&
			echo "Node-kind: file" &&
			echo "Node-action: add" &&
			text_no_props hi
		} >small.dump.r$x
	done &&
	test_set_prereq SMALL_SVNDUMP
'

test_expect_success SMALL_SVNDUMP 'create small svnrepo' '
	rm -rf small.svn* &&
	svnadmin create small.svn &&
	for x in `seq 1 1 10`; do
		cat small.dump.r0 small.dump.r$x >part.dump &&
		svnadmin load small.svn <part.dump &&
		cp -r small.svn small.svn.r0-$x
	done &&
	test_set_prereq SMALL_SVN
'

test_expect_failure EMPTY_SVN 'fetch empty' '
	reinit_git &&
	url=$(svnurl empty.svn) &&
	git remote add svn "$url" &&
	git fetch svn
'

test_expect_failure TINY_SVN 'clone tiny' '
	deinit_git &&
	url=$(svnurl tiny.svn) &&
	git clone "$url" tiny1.git &&
	test_nr_revs 1 tiny1.git refs/remotes/origin/master
'

test_expect_success TINY_SVN 'clone --mirror tiny' '
	deinit_git &&
	url=$(svnurl tiny.svn) &&
	git clone --mirror "$url" tiny2.git &&
	test_nr_revs 1 tiny2.git refs/heads/master
'

test_expect_success TINY_SVN 'clone --bare tiny' '
	deinit_git &&
	url=$(svnurl tiny.svn) &&
	git clone --bare "$url" tiny3.git &&
	test_nr_revs 1 tiny3.git refs/heads/master
'

test_expect_success TINY_SVN 'clone -b master tiny' '
	deinit_git &&
	url=$(svnurl tiny.svn) &&
	git clone -b master "$url" tiny4.git &&
	test_nr_revs 1 tiny4.git/.git refs/heads/master
'

test_expect_success SMALL_SVN 'clone -b master small' '
	deinit_git &&
	url=$(svnurl small.svn) &&
	git clone -b master "$url" small.git &&
	test_nr_revs 10 small.git/.git refs/heads/master
'

test_expect_success TINY_SVN,SVNRDUMP 'no crash on clone url/path' '
	deinit_git &&
	url=$(svnurl small.svn)/directory &&
	git clone -b master "$url" small_dir.git &&
	test_nr_revs 10 small_dir.git/.git refs/heads/master
'

test_expect_success TINY_SVN,SVNRDUMP 'no crash on clone url/path/file' '
	deinit_git &&
	url=$(svnurl small.svn)/directory/somefile3 &&
	git clone -b master "$url" small_dir_file.git &&
	test_nr_revs 10 small_dir_file.git/.git refs/heads/master
'

test_expect_success SMALL_SVN 'fetch each rev of SMALL separately' '
	reinit_git &&
	url=$(svnurl small.svn) &&

	for x in `seq 1 1 10`; do
		git remote add svn_$x "$url.r0-$x"
	done &&
	git remote update &&

	git remote add svn "$url" &&
	git fetch svn &&

	git rev-parse -s remotes/svn_7/master~5 >ref7_2 &&
	git rev-parse -s remotes/svn/master~8 >ref_2 &&
	test_cmp ref7_2 ref_2
'

test_expect_success SMALL_SVN 'fetch updates from SMALL' '
	reinit_git &&
	url=$(svnurl link.svn) &&
	git remote add svn "$url" &&

	ln -sfn small.svn.r0-5 link.svn &&
	git fetch svn &&
	test_nr_revs 5 .git refs/remotes/svn/master &&

	ln -sfn small.svn.r0-10 link.svn &&
	git fetch svn &&
	test_nr_revs 10 .git refs/remotes/svn/master &&

	git fetch svn &&
	test_nr_revs 10 .git refs/remotes/svn/master
'

test_expect_success TINY_SVN 'fetch TINY does not write to refs/heads/master' '
	reinit_git &&
	url=$(svnurl tiny.svn) &&
	git remote add svn "$url" &&
	git fetch svn &&
	git show-ref --verify refs/remotes/svn/master &&
	test_must_fail git show-ref --verify refs/heads/master
'

test_done
