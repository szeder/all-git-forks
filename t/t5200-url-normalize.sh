#!/bin/sh

test_description='url normalization'
. ./test-lib.sh

if test -n "$NO_CURL"; then
	skip_all='skipping test, git built without http support'
	test_done
fi

# Note that only file: URLs should be allowed without a host

test_expect_success 'url scheme' '
	! test-url-normalize "" &&
	! test-url-normalize "_" &&
	! test-url-normalize "scheme" &&
	! test-url-normalize "scheme:" &&
	! test-url-normalize "scheme:/" &&
	! test-url-normalize "scheme://" &&
	! test-url-normalize "file" &&
	! test-url-normalize "file:" &&
	! test-url-normalize "file:/" &&
	test-url-normalize "file://" &&
	! test-url-normalize "://acme.co" &&
	! test-url-normalize "x_test://acme.co" &&
	! test-url-normalize "schem%6e://" &&
	test-url-normalize "x-Test+v1.0://acme.co" &&
	test "$(test-url-normalize -p "AbCdeF://x.Y")" = "abcdef://x.y/"
'

test_expect_success 'url authority' '
	! test-url-normalize "scheme://user:pass@" &&
	! test-url-normalize "scheme://?" &&
	! test-url-normalize "scheme://#" &&
	! test-url-normalize "scheme:///" &&
	! test-url-normalize "scheme://:" &&
	! test-url-normalize "scheme://:555" &&
	test-url-normalize "file://user:pass@" &&
	test-url-normalize "file://?" &&
	test-url-normalize "file://#" &&
	test-url-normalize "file:///" &&
	test-url-normalize "file://:" &&
	! test-url-normalize "file://:555" &&
	test-url-normalize "scheme://user:pass@host" &&
	test-url-normalize "scheme://@host" &&
	test-url-normalize "scheme://%00@host" &&
	! test-url-normalize "scheme://%%@host" &&
	! test-url-normalize "scheme://host_" &&
	test-url-normalize "scheme://user:pass@host/" &&
	test-url-normalize "scheme://@host/" &&
	test-url-normalize "scheme://host/" &&
	test-url-normalize "scheme://host?x" &&
	test-url-normalize "scheme://host#x" &&
	test-url-normalize "scheme://host/@" &&
	test-url-normalize "scheme://host?@x" &&
	test-url-normalize "scheme://host#@x" &&
	test-url-normalize "scheme://[::1]" &&
	test-url-normalize "scheme://[::1]/" &&
	! test-url-normalize "scheme://hos%41/" &&
	test-url-normalize "scheme://[invalid....:/" &&
	test-url-normalize "scheme://invalid....:]/" &&
	! test-url-normalize "scheme://invalid....:[/" &&
	! test-url-normalize "scheme://invalid....:["
'

test_expect_success 'url port checks' '
	test-url-normalize "xyz://q@some.host:" &&
	test-url-normalize "xyz://q@some.host:456/" &&
	! test-url-normalize "xyz://q@some.host:0" &&
	! test-url-normalize "xyz://q@some.host:0000000" &&
	test-url-normalize "xyz://q@some.host:0000001?" &&
	test-url-normalize "xyz://q@some.host:065535#" &&
	test-url-normalize "xyz://q@some.host:65535" &&
	! test-url-normalize "xyz://q@some.host:65536" &&
	! test-url-normalize "xyz://q@some.host:99999" &&
	! test-url-normalize "xyz://q@some.host:100000" &&
	! test-url-normalize "xyz://q@some.host:100001" &&
	test-url-normalize "http://q@some.host:80" &&
	test-url-normalize "https://q@some.host:443" &&
	test-url-normalize "http://q@some.host:80/" &&
	test-url-normalize "https://q@some.host:443?" &&
	! test-url-normalize "http://q@:8008" &&
	! test-url-normalize "http://:8080" &&
	! test-url-normalize "http://:" &&
	test-url-normalize "xyz://q@some.host:456/" &&
	test-url-normalize "xyz://[::1]:456/" &&
	test-url-normalize "xyz://[::1]:/" &&
	! test-url-normalize "xyz://[::1]:000/" &&
	! test-url-normalize "xyz://[::1]:0%300/" &&
	! test-url-normalize "xyz://[::1]:0x80/" &&
	! test-url-normalize "xyz://[::1]:4294967297/" &&
	! test-url-normalize "xyz://[::1]:030f/"
'

test_expect_success 'url port normalization' '
	test "$(test-url-normalize -p "http://x:800")" = "http://x:800/" &&
	test "$(test-url-normalize -p "http://x:0800")" = "http://x:800/" &&
	test "$(test-url-normalize -p "http://x:00000800")" = "http://x:800/" &&
	test "$(test-url-normalize -p "http://x:065535")" = "http://x:65535/" &&
	test "$(test-url-normalize -p "http://x:1")" = "http://x:1/" &&
	test "$(test-url-normalize -p "http://x:80")" = "http://x/" &&
	test "$(test-url-normalize -p "http://x:080")" = "http://x/" &&
	test "$(test-url-normalize -p "http://x:000000080")" = "http://x/" &&
	test "$(test-url-normalize -p "https://x:443")" = "https://x/" &&
	test "$(test-url-normalize -p "https://x:0443")" = "https://x/" &&
	test "$(test-url-normalize -p "https://x:000000443")" = "https://x/"
'

test_expect_success 'url general escapes' '
	! test-url-normalize "http://x.y?%fg" &&
	test "$(test-url-normalize -p "X://W/%7e%41^%3a")" = "x://w/~A%5E%3A" &&
	test "$(test-url-normalize -p "X://W/:/?#[]@")" = "x://w/:/?#[]@" &&
	test "$(test-url-normalize -p "X://W/$&()*+,;=")" = "x://w/$&()*+,;=" &&
	test "$(test-url-normalize -p "X://W/'\''")" = "x://w/'\''" &&
	test "$(test-url-normalize -p "X://W?'\!'")" = "x://w/?'\!'"
';#'

test_expect_success 'url username/password escapes' '
	test "$(test-url-normalize -p "x://%41%62(^):%70+d@foo")" = "x://Ab(%5E):p+d@foo/"
'

test_expect_success 'url normalized lengths' '
	test "$(test-url-normalize -l "Http://%4d%65:%4d^%70@The.Host")" = 25 &&
	test "$(test-url-normalize -l "http://%41:%42@x.y/%61/")" = 17 &&
	test "$(test-url-normalize -l "http://@x.y/^")" = 15
'

test_expect_success 'url . and .. segments' '
	test "$(test-url-normalize -p "x://y/.")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/./")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/a/.")" = "x://y/a" &&
	test "$(test-url-normalize -p "x://y/a/./")" = "x://y/a/" &&
	test "$(test-url-normalize -p "x://y/.?")" = "x://y/?" &&
	test "$(test-url-normalize -p "x://y/./?")" = "x://y/?" &&
	test "$(test-url-normalize -p "x://y/a/.?")" = "x://y/a?" &&
	test "$(test-url-normalize -p "x://y/a/./?")" = "x://y/a/?" &&
	test "$(test-url-normalize -p "x://y/a/./b/.././../c")" = "x://y/c" &&
	test "$(test-url-normalize -p "x://y/a/./b/../.././c/")" = "x://y/c/" &&
	test "$(test-url-normalize -p "x://y/a/./b/.././../c/././.././.")" = "x://y/" &&
	! test-url-normalize "x://y/a/./b/.././../c/././.././.." &&
	test "$(test-url-normalize -p "x://y/a/./?/././..")" = "x://y/a/?/././.." &&
	test "$(test-url-normalize -p "x://y/%2e/")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/%2E/")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/a/%2e./")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/b/.%2E/")" = "x://y/" &&
	test "$(test-url-normalize -p "x://y/c/%2e%2E/")" = "x://y/"
'

# http://@foo specifies an empty user name but does not specify a password
# http://foo  specifies neither a user name nor a password
# So they should not be equivalent
test_expect_success 'url equivalents' '
	test-url-normalize "httP://x" "Http://X/" &&
	test-url-normalize "Http://%4d%65:%4d^%70@The.Host" "hTTP://Me:%4D^p@the.HOST:80/" &&
	! test-url-normalize "https://@x.y/^" "httpS://x.y:443/^" &&
	test-url-normalize "https://@x.y/^" "httpS://@x.y:0443/^" &&
	test-url-normalize "https://@x.y/^/../abc" "httpS://@x.y:0443/abc" &&
	test-url-normalize "https://@x.y/^/.." "httpS://@x.y:0443/"
'

test_done
