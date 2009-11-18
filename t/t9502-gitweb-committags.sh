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
		"Fixes&nbsp;bug&nbsp;<a class=\"text\" href=\"http://bugzilla.example.com/show_bug.cgi?id=1234\">1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

git config gitweb.committag.bugzilla.url 'http://bts.example.com?bug='
test_expect_success 'bugzilla: url overridden but not permitted' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;bug&nbsp;<a class=\"text\" href=\"http://bugzilla.example.com/show_bug.cgi?id=1234\">1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

echo '$committags{"bugzilla"}{"override"} = ["url"];' >> gitweb_config.perl
git config gitweb.committag.bugzilla.url 'http://bts.example.com?bug='
git config gitweb.committag.bugzilla.pattern 'slow DoS regex'
test_expect_success 'bugzilla: url overridden but regex not permitted' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;bug&nbsp;<a class=\"text\" href=\"http://bts.example.com?bug=1234\">1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'
git config --unset gitweb.committag.bugzilla.pattern

echo '$committags{"bugzilla"}{"override"} = 1;' >> gitweb_config.perl
test_expect_success 'bugzilla: url overridden' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"Fixes&nbsp;bug&nbsp;<a class=\"text\" href=\"http://bts.example.com?bug=1234\">1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

git config gitweb.committag.bugzilla.innerpattern ''
git config gitweb.committag.bugzilla.pattern 'Fixes bug (\d+)'
test_expect_success 'bugzilla: pattern overridden' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -F -q \
		"<a class=\"text\" href=\"http://bts.example.com?bug=1234\">Fixes&nbsp;bug&nbsp;1234</a>&nbsp;involving" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

git config --unset gitweb.committag.bugzilla.innerpattern
git config --unset gitweb.committag.bugzilla.pattern
test_expect_success 'bugzilla: affects log view too' '
	gitweb_run "p=.git;a=log" &&
	grep -F -q \
		"<a class=\"text\" href=\"http://bts.example.com?bug=1234\">1234</a>" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 1234 gitweb.output'

echo more_bugzilla > file.txt
git add file.txt
git commit -q -F - file.txt <<END
[#123,#45] This commit fixes two bugs involving bar and baz.
END
git config gitweb.committag.bugzilla.pattern       '^\[#\d+(, ?#\d+)\]'
git config gitweb.committag.bugzilla.innerpattern  '#(\d+)'
git config gitweb.committag.bugzilla.url           'http://bugs/'
test_expect_success 'bugzilla: override everything, use fancier url format' '
       gitweb_run "p=.git;a=commit;h=HEAD" &&
       grep -F -q \
               "[<a class=\"text\" href=\"http://bugs/123\">#123</a>,<a class=\"text\" href=\"http://bugs/45\">#45</a>]" \
               gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 123 gitweb.output'

echo even_more_bugzilla > file.txt
git add file.txt
git commit -q -F - file.txt <<END
Fix memory leak in confabulator from bug 123.

Based on history from bugs 223, 224, and 225,
fix bug 323 or 324.

Bugs:
1234,
1235

Bug: 423,424,425,426,427,428,429,430,431,432,435
Resolves-bugs: #523 #524
END
git config --unset gitweb.committag.bugzilla.pattern
git config --unset gitweb.committag.bugzilla.innerpattern
git config --unset gitweb.committag.bugzilla.url
gitweb_run "p=.git;a=commit;h=HEAD"
test_expect_success 'bugzilla: fancy defaults: match one bug' '
	grep -q "from&nbsp;bug&nbsp;<a[^>]*>123</a>." gitweb.output
'
test_expect_success 'bugzilla: fancy defaults: comma-separated list' '
	grep -q \
		"bugs&nbsp;<a[^>]*>223</a>,&nbsp;<a[^>]*>224</a>,&nbsp;and&nbsp;<a[^>]*>225</a>," \
		gitweb.output
'
test_expect_success 'bugzilla: fancy defaults: or-pair' '
	grep -q "bug&nbsp;<a[^>]*>323</a>&nbsp;or&nbsp;<a[^>]*>324</a>." \
		gitweb.output
'
test_expect_success 'bugzilla: fancy defaults: comma-separated, caps, >10' '
	grep -q \
		"Bug:&nbsp;<a[^>]*>423</a>,<a[^>]*>424</a>,.*,<a[^>]*>435</a>" \
		gitweb.output
'
test_expect_success 'bugzilla: fancy defaults: space-separated with hash' '
	grep -q -e \
		"-bugs:&nbsp;<a[^>]*>#523</a>&nbsp;<a[^>]*>#524</a>" \
		gitweb.output
'
test_expect_success 'bugzilla: fancy defaults: spanning newlines' '
	grep -q -e "<a[^>]*>1234</a>,<br" gitweb.output &&
	grep -q -e "<a[^>]*>1235</a><br" gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep 23 gitweb.output'

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

# ----------------------------------------------------------------------
# custom committags
#
echo custom_test > file.txt
git add file.txt
git commit -q -F - file.txt <<END
Something for <foo&bar@bar.com>
END
echo '$feature{"allowed_committag_subs"}{"default"} = [
	"hyperlink_committag",
	"markup_committag",
	"transform_committag",
	];' >> gitweb_config.perl
git config gitweb.committags 'sha1, obfuscate'
git config gitweb.committag.obfuscate.pattern '([a-z&]+@)[a-z]+(.com)'
git config gitweb.committag.obfuscate.sub 'transform_committag'
git config gitweb.committag.obfuscate.replacement '%2$sXXX%3$s'
test_expect_success 'custom committags: transform_committag' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q -F \
		"foo&amp;bar@XXX.com" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep -F "foo" gitweb.output'

git config gitweb.committags 'sha1, linkemail'
git config gitweb.committag.linkemail.pattern '<([a-z&]+@[a-z]+.com)>'
git config gitweb.committag.linkemail.sub 'markup_committag'
git config gitweb.committag.linkemail.replacement '<a href="mailto:%2$s">%1$s</a>'
test_expect_success 'custom committags: markup_committag' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q -F \
		"<a href=\"mailto:foo&amp;bar@bar.com\">&lt;foo&amp;bar@bar.com&gt;</a>" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep -F "foo" gitweb.output'

echo '$feature{"allowed_committag_subs"}{"default"} = [
	];' >> gitweb_config.perl
test_expect_success 'custom committags: ignored when disabled' '
	gitweb_run "p=.git;a=commit;h=HEAD" &&
	grep -q -F \
		"Something&nbsp;for&nbsp;&lt;foo&amp;bar@bar.com&gt;" \
		gitweb.output
'
test_debug 'cat gitweb.log'
test_debug 'grep -F "foo" gitweb.output'


test_done
