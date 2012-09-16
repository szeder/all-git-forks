#!/bin/sh

test_description='test refspec written by clone-command'
. ./test-lib.sh

test_expect_success 'setup' '
	echo one >file &&
	git add file &&
	git commit -m one &&
	echo two >file &&
	git commit -a -m two &&
	git tag two &&
	echo three >file &&
	git commit -a -m three &&
	git checkout -b foo &&
	echo four >file &&
	git commit -a -m four &&
	git checkout master
'

test_expect_success 'refspec contains all branches by default' '
	git clone "file://$PWD" dir_all &&
	echo "+refs/heads/*:refs/remotes/origin/*" > expected &&
	git --git-dir=dir_all/.git config --get remote.origin.fetch > actual &&
	test_cmp expected actual
'

test_expect_success 'refspec contains only master with option --single-branch and remotes HEAD point to master' '
	git clone --single-branch "file://$PWD" dir_master &&
	echo "+refs/heads/master:refs/remotes/origin/master" > expected &&
	git --git-dir=dir_master/.git config --get remote.origin.fetch > actual &&
	test_cmp expected actual
'

test_expect_success 'refspec contains only foo with option --single-branch and remotes HEAD point to foo' '
	git checkout foo &&
	git clone --single-branch "file://$PWD" dir_foo &&
	echo "+refs/heads/foo:refs/remotes/origin/foo" > expected &&
	git --git-dir=dir_foo/.git config --get remote.origin.fetch > actual &&
	test_cmp expected actual
'

test_expect_success 'refspec contains one branch after using option --single-branch with --branch' '
	git checkout master &&
	git clone --single-branch --branch foo "file://$PWD" dir_foo2 &&
	echo "+refs/heads/foo:refs/remotes/origin/foo" > expected &&
	git --git-dir=dir_foo2/.git config --get remote.origin.fetch > actual &&
	test_cmp expected actual
'

test_expect_success 'no refspec is written if remotes HEAD is detached' '
	git checkout two^ &&
	git clone --single-branch "file://$PWD" dir_detached &&
	rm expected && touch expected &&
	git --git-dir=dir_detached/.git config --get remote.origin.fetch > actual
	test_cmp expected actual
'

test_done
