#!/bin/sh

test_description='narrow misc tests'

. ./test-lib.sh

test_expect_success setup '
	git clone --mirror ../../.git src.git
'

test_expect_success clone '
	git clone --depth=50 --narrow=Documentation --narrow=block-sha1 file://"`pwd`"/src.git dst &&
	test -f dst/Documentation/git.txt &&
	test -f dst/block-sha1/sha1.c &&
	! test -f git.c
'

test_expect_success 'into dst' '
	cd dst
'

#test_expect_success 'fsck' 'git fsck'

test_expect_success 'checkout -b ' '
	git checkout -b mybranch
'

test_expect_success 'log' '
	git log --stat HEAD~20.. >/dev/null
'

test_expect_success 'modify worktree' '
	echo 1 >>Documentation/git.txt
'

test_expect_success 'diff' '
	git diff
'

test_expect_success 'diff HEAD' '
	git diff HEAD
'

test_expect_success 'diff --cached' '
	git add Documentation/git.txt &&
	git diff --cached
'

test_expect_success 'commit' '
	git commit -a -m 1 &&
	git tag branch0 &&
	echo 2 >>block-sha1/sha1.c &&
	git commit -a -m 2 &&
	echo 3 >>Documentation/git.txt &&
	echo 3 >>block-sha1/sha1.c &&
	git commit -a -m 3 &&
	git tag branch1
'

test_expect_success 'internal merge' '
	git checkout branch0 &&
	echo 4 >>block-sha1/sha1.h &&
	git commit -a -m 4 && false
	git tag branch2 &&
	git merge branch1 &&
	grep "^2$" block-sha1/sha1.c &&
	git tag merge1.2
'

test_expect_success 'rebase' '
	git checkout branch1 &&
	git rebase --onto branch2 branch0
'

test_expect_success 'push' '
	git push ../src.git +merge1.2:refs/heads/mybranch &&
	git rev-parse merge1.2 >../expected &&
	(
		cd ../src.git &&
		git rev-parse mybranch >result &&
		test_cmp ../expected result &&
		echo fscking... &&
		git fsck --no-full
	)
'

test_done
