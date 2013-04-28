#!/bin/sh

test_description='testing end-user usability of namespaced remote refs

Set up a local repo with namespaced remote refs, like this:

[remote "origin"]
	fetch = +refs/heads/*:refs/remotes/origin/heads/*
	fetch = +refs/tags/*:refs/remotes/origin/tags/*
	fetch = +refs/notes/*:refs/remotes/origin/notes/*
	fetch = +refs/replace/*:refs/remotes/origin/replace/*
	tagopt = "--no-tags"
	url = ...

Test that the usual end-user operations work as expected with this setup.
'

. ./test-lib.sh

test_expect_success 'setup server repo' '
	git init server &&
	(
		cd server &&
		test_commit server_master_a &&
		git checkout -b other &&
		test_commit server_other_b &&
		git checkout master &&
		test_commit server_master_b
	)
'

server_master_a=$(git --git-dir=server/.git rev-parse --verify server_master_a)
server_master_b=$(git --git-dir=server/.git rev-parse --verify server_master_b)
server_other_b=$(git --git-dir=server/.git rev-parse --verify server_other_b)

cat > expect.refspecs << EOF
+refs/heads/*:refs/remotes/origin/heads/*
+refs/tags/*:refs/remotes/origin/tags/*
+refs/notes/*:refs/remotes/origin/notes/*
+refs/replace/*:refs/remotes/origin/replace/*
EOF

cat > expect.show-ref << EOF
$server_master_b refs/heads/master
$server_master_b refs/remotes/origin/heads/master
$server_other_b refs/remotes/origin/heads/other
$server_master_a refs/remotes/origin/tags/server_master_a
$server_master_b refs/remotes/origin/tags/server_master_b
$server_other_b refs/remotes/origin/tags/server_other_b
EOF

test_clone() {
	( cd $1 && git config --get-all remote.origin.fetch ) > actual.refspecs &&
	test_cmp expect.refspecs actual.refspecs &&
	( cd $1 && git show-ref ) > actual.show-ref &&
	test_cmp expect.show-ref actual.show-ref
}

test_expect_failure 'clone with namespaced remote refs' '
	git clone server client \
		--config remote.origin.fetch="+refs/heads/*:refs/remotes/origin/heads/*" \
		--config remote.origin.fetch="+refs/tags/*:refs/remotes/origin/tags/*" \
		--config remote.origin.fetch="+refs/notes/*:refs/remotes/origin/notes/*" \
		--config remote.origin.fetch="+refs/replace/*:refs/remotes/origin/replace/*" \
		--config remote.origin.tagopt "--no-tags" &&
	test_clone client
'

# Work-around for the above failure
test_expect_success 'work-around "clone" with namespaced remote refs' '
	rm -rf client &&
	git init client &&
	(
		cd client &&
		git config remote.origin.url ../server &&
		git config --add remote.origin.fetch "+refs/heads/*:refs/remotes/origin/heads/*" &&
		git config --add remote.origin.fetch "+refs/tags/*:refs/remotes/origin/tags/*" &&
		git config --add remote.origin.fetch "+refs/notes/*:refs/remotes/origin/notes/*" &&
		git config --add remote.origin.fetch "+refs/replace/*:refs/remotes/origin/replace/*" &&
		git config remote.origin.tagopt "--no-tags" &&
		git fetch &&
		git checkout master
	) &&
	test_clone client
'

test_done
