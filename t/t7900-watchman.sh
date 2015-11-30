#!/bin/sh
#
# Copyright (c) 2014 Twitter, Inc
#

test_description='Watchman'

GIT_TEST_WITH_WATCHMAN=1

. ./test-lib.sh

cat >expect <<\EOF
?? expect
?? morx
?? output
EOF

test_expect_success 'watchman sees new files' '
	touch morx &&
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? expect
?? output
EOF

test_expect_success 'watchman sees file deletion' '
	rm morx &&
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
?? bramp
EOF

test_expect_success 'watchman understands .gitignore' '
	touch bramp &&
	cat >.gitignore <<-EOF &&
	expect*
	output*
EOF
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
EOF

test_expect_success 'watchman notices changes to .gitignore' '
	cat >.gitignore <<-EOF &&
	expect*
	output*
	bramp
EOF
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
EOF

test_expect_success 'watchman understands .git/info/exclude' '
	touch fleem &&
	cat >.git/info/exclude <<-EOF &&
	fleem
EOF
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
?? fleem
EOF

test_expect_success 'watchman notices changes to .git/info/exclude' '
	touch crubbins &&
	cat >.git/info/exclude <<-EOF &&
	crubbins
EOF
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
?? crubbins
?? fleem
EOF

test_expect_success 'watchman notices removal of .git/info/exclude' '
	rm .git/info/exclude &&
	git status -s > output &&
	test_cmp expect output &&
	rm crubbins bramp fleem
'


cat >expect <<\EOF
?? .gitignore
?? fleem
?? myignore
EOF

test_expect_success 'watchman notices changes to file configured by core.excludesfile' '
	touch fleem &&
	touch crubbins &&
	cat >myignore <<-EOF &&
	crubbins
EOF
	git config core.excludesfile myignore &&
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
?? crubbins
?? myignore
?? myignore2
EOF

test_expect_success 'watchman notices changes to config variable core.excludesfile' '
	touch fleem &&
	touch crubbins &&
	cat >myignore2 <<-EOF &&
	fleem
EOF
	git config core.excludesfile myignore2 &&
	git status -s > output &&
	test_cmp expect output
'

cat >expect <<\EOF
?? .gitignore
?? crubbins
?? fleem
?? myignore
EOF

test_expect_success 'watchman notices removal of file referred to by' '
	rm myignore2 &&
	git status -s > output &&
	test_cmp expect output &&
	rm crubbins fleem myignore
'


cat >expect.nothing <<\EOF
EOF

cat >expect.2 <<\EOF
EOF

test_expect_success 'git diff still works' '
	echo 1 > diffy &&
	git add diffy .gitignore &&
	git commit -m initial &&
	git status -s > output &&
	test_cmp expect.nothing output &&
	echo 2 >> diffy &&
	test_cmp expect.2 output
'

cat >expect <<\EOF
 D diffy
EOF

test_expect_success 'file to directory changes still work' '
	rm diffy &&
	mkdir diffy &&
	touch diffy/a &&
	git status -s > output &&
	test_cmp expect output &&
	git add diffy/a &&
	git commit -m two &&
	git status -s > output.nothing
'

cat >expect <<\EOF
 D diffy/a
?? diffy
EOF

test_expect_success 'directory to file changes still work' '
	rm -r diffy &&
	touch diffy &&
	git status -s > output &&
	test_cmp expect output &&
	rm diffy &&
	git rm diffy/a &&
	git commit -m "remove diffy"
'

cat >expect <<\EOF
?? dead
EOF

test_expect_success 'changes while watchman is not running are detected' '
	stop_watchman &&
	touch dead &&
	git status -s > output &&
	test_cmp expect output
'

get_epoch_seconds_mtime() {
	case "$(uname -s)" in
		Darwin) stat -f '%m' "$1" ;;
		Linux) stat -c '%Y' "$1" ;;
		*) error 'unsupported operating system' ;;
	esac
}

test_expect_success 'git deletes stale fs_cache' '
	> .git/fs_cache.lock &&
	touch foo &&
	now=$(date +'%s') &&
	sleep 1 &&
	git status -s >/dev/null &&
	mtime=$(get_epoch_seconds_mtime .git/fs_cache) &&
	test $mtime -le $now &&
	touch -t 201501010000 .git/fs_cache.lock
	git status -s >/dev/null &&
	mtime=$(get_epoch_seconds_mtime .git/fs_cache) &&
	test $mtime -ge $now
'

cat >.watchmanconfig <<\EOF
{
  "ignore_dirs": ["fingle"]
}
EOF

cat >expected <<\EOF
 M .gitignore
?? .watchmanconfig
?? dead
?? foo
EOF

test_expect_success 'git-clean also cleans watchman ignored files' '
	cat >>.gitignore <<-EOF &&
fingle
EOF
	stop_watchman && # restart watchman to pickup the watchmanconfig
	git status -s && # first time cant connect to watchman, regen fs_cache
	git status -s && # second time will get the clock
	mkdir fingle &&
	touch fingle/gorp &&
	git status -s >output &&
	test_cmp output expected &&
	git fs-cache-dump >output &&
	! grep -q fingle output &&
	git clean -fdx &&
	! test -f output &&
	! test -d fingle
'

test_done
