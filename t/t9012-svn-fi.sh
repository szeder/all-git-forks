#!/bin/sh

test_description='check svn dumpfile exporter'

. ./test-lib.sh

svnrepo="testsvn"

if svnadmin --help >/dev/null 2>&1
then
    load_dump () {
	svnadmin load "$1"
    }
    test_set_prereq SVN
fi

reinit_svn () {
	rm -rf "$svnrepo" &&
	rm -f stream &&
	svnadmin create "$svnrepo" &&
	printf "#!/bin/sh" > "$svnrepo"/hooks/pre-revprop-change &&
	chmod +x "$svnrepo"/hooks/pre-revprop-change &&
	mkfifo stream
}

svn_look () {
	subcommand=$1 &&
	shift &&
	svnlook "$subcommand" "$svnrepo" "$@"
}

try_load () {
	input=$1 &&
	maybe_fail=${2:+test_$2} &&

	{
		$maybe_fail test-svn-fi "$input" >stream &
	} &&
	load_dump "$svnrepo" <stream &&
	wait $!
}

test_expect_success SVN 'M: regular files' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 foo
	 bar
	EOF
	cat >expect.cat.foo <<-\EOF &&
	nothing
	EOF
	cat >expect.cat.bar <<-\EOF &&
	nothing again
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline foo
	data 8
	nothing
	M 100644 inline bar
	data 14
	nothing again
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	svn_look cat foo >actual.cat.foo &&
	svn_look cat bar >actual.cat.bar &&
	test_cmp expect.tree actual.tree &&
	test_cmp expect.cat.foo actual.cat.foo &&
	test_cmp expect.cat.bar actual.cat.bar
'

test_expect_success SVN 'D: regular files' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline foo
	data 7
	nothing
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	D foo
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'D: directories' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline subdir/foo
	data 7
	nothing
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	D subdir
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'R: regular files' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 bar
	EOF
	cat >expect.cat <<-\EOF &&
	nothing
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline foo
	data 8
	nothing
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 6
	rename
	R foo bar
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	svn_look cat bar >actual.cat &&
	test_cmp expect.tree actual.tree &&
	test_cmp expect.cat actual.cat
'

test_expect_success SVN 'R: directories' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 subdir2/
	  foo
	  bar
	EOF
	cat >expect.cat.foo <<-\EOF &&
	nothing
	EOF
	cat >expect.cat.bar <<-\EOF &&
	nothing again
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline subdir/foo
	data 8
	nothing
	M 100644 inline subdir/bar
	data 14
	nothing again
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 12
	replace root
	R subdir subdir2
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	svn_look cat subdir2/foo >actual.cat.foo &&
	svn_look cat subdir2/bar >actual.cat.bar &&
	test_cmp expect.tree actual.tree &&
	test_cmp expect.cat.foo actual.cat.foo &&
	test_cmp expect.cat.bar actual.cat.bar
'

test_expect_success SVN 'C: regular files' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 foo
	 bar
	EOF
	cat >expect.cat <<-\EOF &&
	nothing
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline foo
	data 8
	nothing
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 6
	rename
	C foo bar
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	svn_look cat foo >actual.cat.foo &&
	svn_look cat bar >actual.cat.bar &&
	test_cmp expect.tree actual.tree &&
	test_cmp expect.cat actual.cat.foo &&
	test_cmp expect.cat actual.cat.bar
'

test_expect_success SVN 'C: directories' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 subdir/
	  foo
	  bar
	 subdir2/
	  foo
	  bar
	EOF
	cat >expect.cat.foo <<-\EOF &&
	nothing
	EOF
	cat >expect.cat.bar <<-\EOF &&
	nothing again
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 100644 inline subdir/foo
	data 8
	nothing
	M 100644 inline subdir/bar
	data 14
	nothing again
	commit refs/heads/master
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 12
	replace root
	C subdir subdir2
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	svn_look cat subdir2/foo >actual.cat.foo &&
	svn_look cat subdir2/bar >actual.cat.bar &&
	test_cmp expect.tree actual.tree &&
	test_cmp expect.cat.foo actual.cat.foo &&
	test_cmp expect.cat.bar actual.cat.bar
'

test_expect_success SVN 'ignore checkpoint, progress, feature, option' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	EOF
	cat >input <<-EOF &&
	checkpoint
	progress 3
	feature foo
	option bar
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'ignore tag' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 foo
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	M 644 inline foo
	data 0
	tag moo
	from master
	tagger <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	data 8
	nothing
	tag bar
	from master
	tagger $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	data 10
	something
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'ignore notemodify' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	something
	N moo :1
	data 8
	nothing
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'svn:special and svn:executable' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 644 inline baz
	data 0
	M 100755 inline foo
	data 0
	M 755 inline moo
	data 0
	M 120000 inline bar
	data 0
	EOF
	try_load input &&
	svn_look propget svn:executable foo &&
	svn_look propget svn:executable moo &&
	svn_look propget svn:special bar
'

test_expect_success SVN 'invalid toplevel command' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	marks :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'invalid command after commit' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	datum 0
	M 100644 inline foo
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'empty log, empty file' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 foo
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'missing lf after data' '
	reinit_svn &&
	test_tick &&
	cat >expect.log <<-\EOF &&
	something
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 9
	somethingM 100644 inline foo
	data 0
	EOF
	try_load input &&
	svn_look log >actual.log &&
	test_cmp expect.log actual.log
'

test_expect_success SVN 'revprops: svn:author, svn:log' '
	reinit_svn &&
	test_tick &&
	echo "nothing" >expect.log &&
	echo "author" >expect.author &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <author@example.com> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 100644 inline foo
	data 0
	EOF
	try_load input &&
	svn_look log >actual.log &&
	svn_look author >actual.author &&
	test_cmp expect.log actual.log &&
	test_cmp expect.author actual.author
'

test_expect_success SVN 'missing author line' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 foo
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'author email without @' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <example> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	try_load input
'

test_expect_success SVN 'blob marks unsupported' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	blob
	mark :1
	data 0
	reset refs/heads/master
	commit refs/heads/master
	mark :2
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 :1 foo
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'malformed filemodify line' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'malformed author line' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author 2d3%*s&f#k|
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'missing committer line' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	data 0
	M 100644 inline foo
	data 0
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'malformed data length' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_AUTHOR_DATE
	data 0
	M 100644 inline foo
	data 14238
	EOF
	test_must_fail try_load input
'

test_expect_success SVN 'recursive directory creation' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 alpha/
	  beta/
	   gamma
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 100644 inline alpha/beta/gamma
	data 12
	some content
	EOF
	try_load input &&
	svn_look tree >actual.tree &&
	test_cmp expect.tree actual.tree
'

test_expect_success SVN 'replace symlink with normal file' '
	reinit_svn &&
	test_tick &&
	cat >expect.tree <<-\EOF &&
	/
	 alpha/
	  beta/
	   gamma
	EOF
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 120000 inline alpha/beta/gamma
	data 0
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 100644 inline alpha/beta/gamma
	data 0
	EOF
	try_load input &&
	svn_look tree -r1 >actual.tree1 &&
	svn_look tree -r2 >actual.tree2 &&
	test_cmp expect.tree actual.tree1 &&
	test_cmp expect.tree actual.tree2
'

test_expect_success SVN 'path includes symlink' '
	reinit_svn &&
	test_tick &&
	cat >input <<-EOF &&
	reset refs/heads/master
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 120000 inline alpha/beta/gamma
	data 0
	commit refs/heads/master
	mark :1
	author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE
	committer $GIT_COMMITTER_NAME <$GIT_COMMITTER_EMAIL> $GIT_COMMITTER_DATE
	data 7
	nothing
	M 100644 inline alpha/beta/gamma/bar
	data 0
	EOF
	test_must_fail try_load input
'

test_done
