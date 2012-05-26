#!/bin/sh

test_description='git svn messages prog tests'

. ./lib-git-svn.sh

cat > svn-messages-prog <<'EOF'
#!/bin/sh
sed s/foo/bar/g
EOF
chmod +x svn-messages-prog

test_expect_success 'setup svnrepo' '
	svn mkdir -m "Unchanged message" "$svnrepo"/a
	svn mkdir -m "Changed message: foo" "$svnrepo"/b
	'

test_expect_success 'import messages with prog' '
	git svn clone --messages-prog=./svn-messages-prog \
	    "$svnrepo" x
	'

test_expect_success 'imported 2 revisions successfully' '
	(
		cd x
		test "`git rev-list refs/remotes/git-svn | wc -l`" -eq 2
	)
	'

test_expect_success 'messages-prog ran correctly' '
	(
		cd x
		git rev-list -1 --pretty=raw refs/remotes/git-svn~1 | \
		  grep "^    Unchanged message" &&
		git rev-list -1 --pretty=raw refs/remotes/git-svn~0 | \
		  grep "^    Changed message: bar"
	)
	'

test_done
