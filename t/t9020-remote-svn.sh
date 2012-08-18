#!/bin/sh

test_description='tests remote-svn'

. ./test-lib.sh

# We override svnrdump by placing a symlink to the svnrdump-emulator in .
export PATH="$HOME:$PATH"
ln -sf $GIT_BUILD_DIR/contrib/svn-fe/svnrdump_sim.py "$HOME/svnrdump"

init_git () {
	rm -fr .git &&
	git init &&
	#git remote add svnsim svn::sim:///$TEST_DIRECTORY/t9020/example.svnrdump
	# let's reuse an exisiting dump file!?
	git remote add svnsim svn::sim:///$TEST_DIRECTORY/t9154/svn.dump
	git remote add svnfile svn::file:///$TEST_DIRECTORY/t9154/svn.dump
}

test_debug '
	git --version
	which git
	which svnrdump
'

test_expect_success 'simple fetch' '
	init_git &&
	git fetch svnsim &&
	test_cmp .git/refs/svn/svnsim/master .git/refs/remotes/svnsim/master  &&
	cp .git/refs/remotes/svnsim/master master.good
'

test_debug '
	cat .git/refs/svn/svnsim/master
	cat .git/refs/remotes/svnsim/master
'

test_expect_success 'repeated fetch, nothing shall change' '
	git fetch svnsim &&
	test_cmp master.good .git/refs/remotes/svnsim/master
'

test_expect_success 'fetch from a file:// url gives the same result' '
	git fetch svnfile
'

test_expect_failure 'the sha1 differ because the git-svn-id line in the commit msg contains the url' '
	test_cmp .git/refs/remotes/svnfile/master .git/refs/remotes/svnsim/master
'

test_expect_success 'mark-file regeneration' '
	mv .git/info/fast-import/remote-svn/svnsim.marks .git/info/fast-import/remote-svn/svnsim.marks.old &&
	git fetch svnsim &&
	test_cmp .git/info/fast-import/remote-svn/svnsim.marks.old .git/info/fast-import/remote-svn/svnsim.marks
'

test_expect_success 'incremental imports must lead to the same head' '
	export SVNRMAX=3 &&
	init_git &&
	git fetch svnsim &&
	test_cmp .git/refs/svn/svnsim/master .git/refs/remotes/svnsim/master  &&
	unset SVNRMAX &&
	git fetch svnsim &&
	test_cmp master.good .git/refs/remotes/svnsim/master
'

test_debug 'git branch -a'

test_done
