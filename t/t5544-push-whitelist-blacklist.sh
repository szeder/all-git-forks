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

test_expect_success 'default behavior' '
	test_might_fail git push http://example.com HEAD 2> result &&
	test_must_fail test_cmp result forbidden
'

test_expect_success 'basic case' '
	test_config remote.pushBlacklist http://example.com &&
	test_must_fail git push http://example.com HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'default message set' '
	test_config remote.pushDenyMessage "new deny message" &&
	echo "fatal: new deny message" > newDenyMessage &&
	test_config remote.pushBlacklist http://example.com &&
	test_must_fail git push http://example.com HEAD 2> result &&
	test_cmp result newDenyMessage
'

test_expect_success 'no scheme blacklist' '
	test_config remote.pushBlacklist example.com &&
	test_must_fail git push http://example.com HEAD 2> result &&
	test_cmp result forbidden
'


test_expect_success 'no scheme whitelist' '
	test_config remote.pushDefaultPolicy deny &&
	test_config remote.pushWhitelist example.com &&
	test_might_fail git push http://example.com HEAD 2> result &&
	test_must_fail test_cmp result forbidden
'

test_expect_success 'local path' '
	test_config remote.pushBlacklist $(pwd)/blacklist &&
	test_must_fail git push $(pwd)/blacklist HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'local path with file://' '
	test_config remote.pushBlacklist file://$(pwd)blacklist &&
	test_must_fail git push file://$(pwd)/blacklist HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'only one scheme allowed' '
	test_config remote.pushBlacklist example.com &&
	test_config remote.pushWhitelist https://example.com &&
	test_might_fail git push https://example.com HEAD 2> result &&
	test_must_fail test_cmp result forbidden
'

test_expect_success 'only one scheme denied' '
	test_config remote.pushDefaultPolicy deny &&
	test_config remote.pushWhitelist example.com &&
	test_config remote.pushBlacklist https://example.com &&
	test_must_fail git push https://example.com HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'allowed repo in denied repo' '
	test_config remote.pushBlacklist $(pwd)/blacklist &&
	test_config remote.pushWhitelist $(pwd)/blacklist/allow &&
	git push blacklist/allow HEAD
'

test_expect_success 'multiple patterns matchs' '
	test_config remote.pushBlacklist http://example.com &&
	test_config remote.pushWhitelist http://example.com/blacklist/allow &&
	git config --add remote.pushBlacklist http://example.com/blacklist/allow/deny &&
	test_must_fail git push http://example.com/blacklist/allow/deny HEAD 2> result &&
	test_cmp result forbidden
'

test_expect_success 'all schemes denied but one' '
	test_config remote.pushBlacklist example.com &&
	test_config remote.pushWhitelist https://example.com &&
	test_might_fail git push https://example.com HEAD 2> result &&
	test_must_fail test_cmp result forbidden
'

test_expect_success 'all schemes allowed but one' '
	test_config remote.pushDefaultPolicy deny &&
	test_config remote.pushWhitelist example.com &&
	test_config remote.pushBlacklist https://example.com &&
	test_must_fail git push https://example.com HEAD 2> result &&
	test_cmp result forbidden
'

test_done
