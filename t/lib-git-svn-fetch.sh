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
	svn "$orig_svncmd" --username user --password pass --no-auth-cache --non-interactive --config-dir "$svnconf" "$@"
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

function test_subject() {
	commit="$1"
	subject="$2"
	commit_subject="`git log -1 --pretty=format:%s $commit`"
	test "$commit_subject" == "$subject"
}

function test_author() {
	commit="$1"
	author="$2"
	commit_author="`git log -1 --pretty=format:'%an <%ae>' $commit`"
	test "$commit_author" == "$author"
}

function test_date() {
	commit="$1"
	date="$2"
	commit_date="`git log -1 --pretty=format:%ai $commit | sed -re 's#^([^ ]*) ([^ ]*) \+0000$#\1T\2Z#g'`"
	test "$commit_date" == "$date"
}

function setup_branches() {
	rm -rf svnco &&
	svn_cmd co $svnurl svnco &&
	cd svnco &&
	svn_cmd mkdir Trunk &&
	svn_cmd mkdir Random &&
	svn_cmd mkdir Branches &&
	svn_cmd mkdir Tags &&
	touch Trunk/file.txt Random/file.txt &&
	svn_cmd add Random/file.txt &&
	svn_cmd add Trunk/file.txt &&
	svn_cmd ci -m "init" &&
	svn_cmd up &&
	echo "other" >> Trunk/file.txt &&
	svn_cmd ci -m "trunk file" &&
	svn_cmd up &&
	echo "contents" >> Random/file.txt &&
	svn_cmd ci -m "create random file" &&
	svn_cmd up &&
	svn_cmd copy Trunk Branches/CopiedBranch &&
	echo "more" >> Branches/CopiedBranch/file2.txt &&
	svn_cmd add Branches/CopiedBranch/file2.txt &&
	svn_cmd ci -m "create copied branch" &&
	svn_cmd up &&
	svn_cmd mkdir Branches/NonCopiedBranch &&
	echo "non copied" >> Branches/NonCopiedBranch/file.txt &&
	svn_cmd add Branches/NonCopiedBranch/file.txt &&
	svn_cmd ci -m "create non-copied branch"
	svn_cmd up &&
	cd ..
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
user = pass
!
	cat > .git/svn-authors <<!
user:pass = Full Name <mail@example.com>
!
	svnserve --daemon \
		--listen-port $SVNSERVE_PORT \
		--root "$svnrepo" \
		--listen-host localhost &&
	git config svn.user user &&
	git config svn.url $svnurl &&
	git config svn.remote svn
'
