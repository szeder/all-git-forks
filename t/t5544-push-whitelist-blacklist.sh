#!/bin/sh

test_description='push allowed/denied depending on config options'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit commit &&
	git init --bare wlisted1 &&
	git init --bare wlisted1/folder &&
	git init --bare wlisted1/folder/blistedfolder &&
	git init --bare wlisted1/folder/blistedfolder/folder &&
	git init --bare wlisted2 &&
	git init --bare blisted1 &&
	git init --bare blisted1/folder &&
	git init --bare blisted2 &&
	git init --bare untracked &&
	git init --bare repo &&
	git remote add wlisted wlisted1 &&
	git remote add wlisted2 wlisted2 &&
	git remote add blisted blisted1 &&
	git remote add blisted2 blisted2 &&
	git remote add repo repo &&
	git config --add remote.pushWhitelisted wlisted1 &&
	git config --add remote.pushWhitelisted wlisted2 &&
	git config --add remote.pushWhitelisted repo &&
	git config --add remote.pushBlacklisted blisted1 &&
	git config --add remote.pushBlacklisted blisted2 &&
	git config --add remote.pushBlacklisted repo &&
	git config --add remote.pushBlacklisted wlisted1/folder/blistedfolder
'

test_expect_success 'whitelist/blacklist with default pushDefaultPolicy' '
	git push wlisted1 HEAD &&
	git push wlisted2 HEAD &&
	test_must_fail git push blisted1 HEAD &&
	test_must_fail git push blisted2 HEAD &&
	git push untracked HEAD
'

test_expect_success 'whitelist/blacklist with allow pushDefaultPolicy' '
	test_config remote.pushDefaultPolicy allow &&
	git push wlisted1 HEAD &&
	git push wlisted2 HEAD &&
	test_must_fail git push blisted1 HEAD &&
	test_must_fail git push blisted2 HEAD &&
	git push untracked HEAD
'

test_expect_success 'whitelist/blacklist with deny pushDefaultPolicy' '
	test_config remote.pushDefaultPolicy deny &&
	git push wlisted1 HEAD &&
	git push wlisted2 HEAD &&
	test_must_fail git push blisted1 HEAD &&
	test_must_fail git push blisted2 HEAD &&
	test_must_fail git push untracked HEAD
'

test_expect_success 'remote is whitelisted and blacklisted at the same time exception' '
	test_must_fail git push repo HEAD 2> result &&
	echo "fatal: repo cannot be whitelisted and blacklisted at the same time" > expected &&
	test_cmp result expected
'

test_expect_success 'remote rejected default message' '
	test_must_fail git push blisted1 HEAD 2> result &&
	echo "fatal: Pushing to this remote is forbidden." > expected &&
	test_cmp result expected
'

test_expect_success 'remote rejected custom message' '
	test_config remote.pushDenyMessage "denied" &&
	test_must_fail git push blisted1 HEAD 2> result &&
	echo "fatal: denied" > expected &&
	test_cmp result expected
'

test_expect_success 'push accepted/rejected depending on config precision' '
	test_config remote.pushDefaultPolicy deny &&
	git push wlisted1/folder HEAD &&
	test_must_fail git push blisted1/folder HEAD &&
	test_must_fail git push wlisted1/folder/blistedfolder/folder HEAD
'

test_expect_success 'unsetup' '
	test_unconfig remote.pushBlackListed &&
	test_unconfig remote.pushWhiteListed
'

test_done
