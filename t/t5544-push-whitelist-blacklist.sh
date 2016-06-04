#!/bin/sh

test_description='blacklist for push'

. ./test-lib.sh

test_expect_success 'setup' '
	git init --bare blacklist/ &&
	git init --bare whitelist/ &&
	git init --bare blacklist/allow &&
	test_commit commit &&
	echo "fatal: Pushing to this remote using this protocol is forbidden" > forbidden
'

test_expect_success 'basic case' '
	git config --add remote.pushBlacklist http://blacklist.com &&
	test_must_fail git push http://blacklist.com HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'no scheme and no path' '
	git config remote.pushBlacklist blacklist.com &&
	test_must_fail git push http://blacklist.com/foo HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'local path' '
	git config remote.pushBlacklist blacklist &&
	test_must_fail git push blacklist HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'local path with file://' '
	git config remote.pushBlacklist file://blacklist &&
	test_must_fail git push blacklist HEAD 2> result &&
	test_cmp result forbidden
'
test_expect_success 'only one scheme allowed' '
	git config remote.pushDefaultPolicy deny &&
	git config remote.pushWhitelist http://blacklist.com &&
	test_must_fail git push https://blacklist.com HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'denied repo in allowed repo' '
	git config remote.pushBlacklist blacklist &&
	git config --add remote.pushWhitelist blacklist/allow &&
	git push blacklist/allow HEAD
'

test_done
