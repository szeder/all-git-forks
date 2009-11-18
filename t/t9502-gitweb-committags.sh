#!/bin/sh

test_description='gitweb committag tests.

This test runs gitweb (git web interface) as CGI script from
commandline, and checks that committags perform the expected
transformations on log messages.'

. ./gitweb-lib.sh

# ----------------------------------------------------------------------
# sha1 linking
#
echo sha1_test > file.txt
git add file.txt
git commit -q -F - file.txt <<END
Summary

See also commit 567890ab
END
test_expect_success 'sha1 link: enabled by default' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q \
		"commit&nbsp;<a class=\"text\" href=\".*\">567890ab</a>" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 567890ab gitweb.output'

# ----------------------------------------------------------------------
# bugzilla commit tag
#

echo bugzilla_test > file.txt
git add file.txt
git commit -q -F - file.txt <<END
Fix foo

Fixes bug 1234 involving foo.
END
git config gitweb.committags 'sha1, bugzilla'
test_expect_success 'bugzilla: enabled but not permitted' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;bug&nbsp;1234&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

echo '$feature{"committags"}{"override"} = 1;' >> gitweb_config.perl
test_expect_success 'bugzilla: enabled' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;<a class=\"text\" href=\"http://bugzilla.example.com/show_bug.cgi?id=1234\">bug&nbsp;1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

git config gitweb.committag.bugzilla.url 'http://bts.example.com?bug='
test_expect_success 'bugzilla: url overridden but not permitted' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;<a class=\"text\" href=\"http://bugzilla.example.com/show_bug.cgi?id=1234\">bug&nbsp;1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

echo '$committags{"bugzilla"}{"override"} = 1;' >> gitweb_config.perl
test_expect_success 'bugzilla: url overridden' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;<a class=\"text\" href=\"http://bts.example.com?bug=1234\">bug&nbsp;1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

git config gitweb.committag.bugzilla.pattern 'Fixes bug (\d+)'
test_expect_success 'bugzilla: pattern overridden' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"<a class=\"text\" href=\"http://bts.example.com?bug=1234\">Fixes&nbsp;bug&nbsp;1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'
git config --unset gitweb.committag.bugzilla.pattern

test_expect_success 'bugzilla: affects log view too' '
	gitweb_run "p=.git;a=log" &&
	grep -F -q \
		"<a class=\"text\" href=\"http://bts.example.com?bug=1234\">bug&nbsp;1234</a>" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

# ----------------------------------------------------------------------
# url linking
#
echo url_test > file.txt
git add file.txt
url='http://user@pass:example.com/foo.html?u=v&x=y#z'
url_esc="$(echo "$url" | sed 's/&/&amp;/g')"
git commit -q -F - file.txt <<END
Summary

See also $url.
END
echo '$feature{"committags"}{"override"} = 1;' >> gitweb_config.perl
git config gitweb.committags 'sha1, url'
test_expect_success 'url link: links when enabled' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q -F \
		"See&nbsp;also&nbsp;<a class=\"text\" href=\"$url_esc\">$url_esc</a>." \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep -F "$url" gitweb.output'

# ----------------------------------------------------------------------
# message id linking
#
echo msgid_test > file.txt
git add file.txt
url='http://mid.gmane.org/'
msgid='<x@y.z>'
msgid_esc="$(echo "$msgid" | sed 's/</\&lt;/g; s/>/\&gt;/g')"
msgid_url="$url$(echo "$msgid" | sed 's/</%3C/g; s/@/%40/g; s/>/%3E/g')"
git commit -q -F - file.txt <<END
Summary

See msg-id $msgid.
END
echo '$feature{"committags"}{"override"} = 1;' >> gitweb_config.perl
git config gitweb.committags 'sha1, messageid'
test_expect_success 'msgid link: linked when enabled' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q -F \
		"See&nbsp;<a class=\"text\" href=\"$msgid_url\">msg-id&nbsp;$msgid_esc</a>." \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep -F "y.z" gitweb.output'


test_done
