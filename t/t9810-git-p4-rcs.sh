#!/bin/sh

test_description='git-p4 rcs keywords'

. ./lib-git-p4.sh

test_expect_success 'start p4d' '
	start_p4d
'

create_kw_file () {
	cat <<\EOF >"$1"
/* A file
	Id: $Id$
	Revision: $Revision$
	File: $File$
 */
int main(int argc, const char **argv) {
	return 0;
}
EOF
}

test_expect_success 'init depot' '
	(
		cd "$cli" &&
		echo file1 >file1 &&
		p4 add file1 &&
		p4 submit -d "change 1" &&
		create_kw_file kwfile1.c &&
		p4 add kwfile1.c &&
		p4 submit -d "Add rcw kw file" kwfile1.c
	)
'

p4_append_to_file () {
	f=$1 &&
	p4 edit -t ktext "$f" &&
	echo "/* $(date) */" >>"$f" &&
	p4 submit -d "appending a line in p4"
}

# Create some files with RCS keywords. If they get modified
# elsewhere then the version number gets bumped which then
# results in a merge conflict if we touch the RCS kw lines,
# even though the change itself would otherwise apply cleanly.
test_expect_failure 'cope with rcs keyword expansion damage' '
	"$GITP4" clone --dest="$git" //depot &&
	(
		cd "$git" &&
		git config git-p4.skipSubmitEdit true &&
		git config git-p4.attemptRCSCleanup true &&
		(cd ../cli && p4_append_to_file kwfile1.c) &&
		perl -n -i -e "print unless m/Revision:/" kwfile1.c &&
		git add kwfile1.c &&
		git commit -m "Zap an RCS kw line" &&
		"$GITP4" submit &&
		"$GITP4" rebase &&
		git diff p4/master &&
		"$GITP4" commit &&
		echo "try modifying in both" &&
		cd "$cli" &&
		p4 edit kwfile1.c &&
		echo "line from p4" >>kwfile1.c &&
		p4 submit -d "add a line in p4" kwfile1.c &&
		cd "$git" &&
		echo "line from git at the top" | cat - kwfile1.c >kwfile1.c.new &&
		mv kwfile1.c.new kwfile1.c &&
		git commit -m "Add line in git at the top" kwfile1.c &&
		"$GITP4" rebase &&
		"$GITP4" submit
	) &&
	rm -rf "$git" && mkdir "$git"
'


test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
