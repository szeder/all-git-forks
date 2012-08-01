#!/bin/sh

. ./test-lib.sh

if test -z "$SVNSERVE_PORT"
then
	skip_all='skipping svnserve test. (set $SVNSERVE_PORT to enable)'
	test_done
fi

svnrepo=$PWD/svnrepo
svnconf=$PWD/svnconf
svnurl="svn://localhost:$SVNSERVE_PORT"

# We need this, because we should pass empty configuration directory to
# the 'svn commit' to avoid automated property changes and other stuff
# that could be set from user's configuration files in ~/.subversion.
svn_cmd () {
	[ -d "$svnconf" ] || mkdir "$svnconf"
	cat > "$svnconf/servers" <<!
[global]
store-plaintext-passwords = yes
!
	orig_svncmd="$1"; shift
	if [ -z "$orig_svncmd" ]; then
		svn
		return
	fi
	svn "$orig_svncmd" --username committer --password pass --no-auth-cache --non-interactive --config-dir "$svnconf" "$@"
}

function test_file() {
	file_contents="`cat $1`"
	test_contents="$2"
	test "$file_contents" == "$test_contents"
}

function svn_date() {
	revision="$1"
	directory="$2"
	svn_cmd log -r "$revision" --xml -l 1 "$directory" | grep "<date>" | sed -re 's#^<date>([^\.Z]*)\.[0-9]+Z</date>#\1Z#g'
}

function test_git_subject() {
	commit="$1"
	subject="$2"
	commit_subject="`git log -1 --pretty=format:%s $commit`"
	echo test_git_subject "$commit_subject" "$subject"
	test "$commit_subject" == "$subject"
}

function test_git_author() {
	commit="$1"
	author="$2"
	commit_author="`git log -1 --pretty=format:'%an <%ae>' $commit`"
	echo test_git_author "$commit_author" "$author"
	test "$commit_author" == "$author"
}

function test_git_date() {
	commit="$1"
	date="$2"
	commit_date="`git log -1 --pretty=format:%ai $commit | sed -re 's#^([^ ]*) ([^ ]*) \+0000$#\1T\2Z#g'`"
	echo test_git_date "$commit_date" "$date"
	test "$commit_date" == "$date"
}

function test_svn_subject() {
	revision="$1"
	subject="$2"
	commit_subject="`svn log -l 1 --xml -r $revision | grep '<msg>' | sed -re 's#<msg>(.*)#\1#g'`"
	echo test_svn_subject "$commit_subject" "$subject"
	test "$commit_subject" == "$subject"
}

function test_svn_author() {
	revision="$1"
	author="$2"
	commit_author="`svn log -l 1 --xml -r $revision | grep '<author>' | sed -re 's#<author>(.*)</author>#\1#g'`"
	echo test_svn_author "$commit_author" "$author"
	test "$commit_author" == "$author"
}

show_ref() {
	(git show-ref --head $1 | cut -d ' ' -f 1) || echo $1
}

show_tag() {
	show_ref refs/tags/$1 | git cat-file --batch | grep object | cut -f 2 -d ' '
}

merge_base() {
	git merge-base `show_ref $1` `show_ref $2`
}

test_expect_success 'start svnserve' '
	killall svnserve &> /dev/null
	killall lt-svnserve &> /dev/null
	rm -rf "$svnrepo" &&
	mkdir -p "$svnrepo" &&
	svnadmin create "$svnrepo" &&
	cat > "$svnrepo/conf/svnserve.conf" <<!
[general]
auth-access = write
password-db = passwd
!
	cat > "$svnrepo/conf/passwd" <<!
[users]
committer = pass
!
	cat > .git/svn-authors <<!
committer:pass = C O Mitter <committer@example.com>
!
	svnserve --daemon \
		--listen-port $SVNSERVE_PORT \
		--root "$svnrepo" \
		--listen-host localhost &&
	git config svn.user committer &&
	git config svn.url $svnurl &&
	git config svn.remote svn &&
	svn_cmd co $svnurl svnco
'
