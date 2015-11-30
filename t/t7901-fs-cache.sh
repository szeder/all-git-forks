#!/bin/sh
#
# Copyright (c) 2014 Twitter, Inc
#

test_description='fs cache validation'

GIT_TEST_WITH_WATCHMAN=1

. ./test-lib.sh

if ! test_have_prereq WATCHMAN
then
	skip_all='skipping watchman tests - no watchman'
	test_done
fi

cat >.gitignore <<\EOF
.gitignore
.gitdumps
output
actual
expect
EOF

test_expect_success 'fs_cache is created' '
	git add -f .gitignore &&
	git commit -m "initial commit" &&
	test_path_is_file .git/fs_cache
'

cat >expect <<\EOF
morx	0	100644
EOF

test_expect_success 'fs_cache contains newly-created files' '
	touch morx &&
	git status &&
	git fs-cache-dump -vg > output &&
	grep "^morx" output > actual &&
	test_cmp expect actual
'

test_expect_success 'fs_cache reflects file deletion' '
	rm morx &&
	git status &&
	git fs-cache-dump -vg > output &&
	! grep "^morx" output
'

test_expect_success 'fs_cache is consistent after file modification' '
	touch nizbix &&
	git status &&
	echo "blurg" >> nizbix &&
	git status &&
	git fs-cache-dump -vg
'

test_expect_success 'fs_cache is consistent after overwrite' '
	ls > topso &&
	git status &&
	echo "" > topso &&
	git status &&
	git fs-cache-dump -vg
'
cat >expect <<\EOF
doxi	0	100644
EOF

test_expect_success 'fs_cache tracks file rename' '
	touch borgis &&
	git status &&
	mv borgis doxi &&
	git status &&
	git fs-cache-dump -vg > output &&
	grep "^doxi" output > actual &&
	! grep "^borgis" output &&
	test_cmp expect actual
'
cat >expect <<\EOF
dorx	0	40755
EOF

test_expect_success 'fs_cache contains newly-created dir' '
	mkdir dorx &&
	git status &&
	git fs-cache-dump -vg > output &&
	grep "^dorx" output > actual &&
	test_cmp expect actual
'

test_expect_success 'fs_cache reflects dir removal' '
	rmdir dorx &&
	git status &&
	git fs-cache-dump -vg > output &&
	! grep "^dorx" output
'

cat >expect <<\EOF
dizbix/abb	0	100644
EOF

test_expect_success 'fs_cache is consistent after file added to dir' '
	mkdir dizbix &&
	touch dizbix/abb &&
	git status &&
	git fs-cache-dump -vg > output &&
	grep "^dizbix/abb" output > actual &&
	test_cmp expect actual &&
	git status &&
	git fs-cache-dump -vg
'

test_expect_success 'fs_cache reflects file removed from dir' '
	rm dizbix/abb &&
	git status &&
	git fs-cache-dump -g > output &&
	! grep "^dizbix/abb" output
'

test_expect_success 'fs_cache is consistent after file removed from dir' '
	mkdir fizzbuzz &&
	touch fizzbuzz/zerp &&
	git status &&
	rm fizzbuzz/zerp &&
	git status &&
	git fs-cache-dump -vg
'

cat >expect <<\EOF
dorgis/dipi	0	100644
EOF

test_expect_success 'fs_cache tracks file renamed in dir' '
	mkdir dorgis &&
	touch dorgis/abc &&
	git status &&
	mv dorgis/abc dorgis/dipi &&
	git status &&
	git fs-cache-dump -g > output &&
	! grep "^dorgis/abc" output &&
	grep "^dorgis/dipi" output > actual &&
	test_cmp expect actual
'

test_expect_success 'fs_cache is consistent after file renamed in dir' '
	mkdir forgis &&
	touch forgis/abc &&
	git status &&
	mv forgis/abc forgis/dipi &&
	git status &&
	git fs-cache-dump -vg
'

test_expect_success 'consistent fs_cache after checking out a hard link' '
	git checkout master &&
	(git branch -D branch || true) &&
	git checkout -b branch &&
	touch a &&
	ln a b &&
	git add a b &&
	git commit -m "normal link" &&
	git checkout master &&
	git fs-cache-dump -vg &&
	git checkout branch &&
	git fs-cache-dump -vg
'

test_expect_success 'consistent fs_cache after checking out a soft link' '
	git checkout master &&
	(git branch -D branch || true) &&
	git checkout -b branch &&
	touch a &&
	ln -s a b &&
	git add a b &&
	git commit -m "normal link" &&
	git checkout master &&
	git fs-cache-dump -vg &&
	git checkout branch &&
	git fs-cache-dump -vg
'

test_expect_success 'consistent fs_cache after checking out an executable file' '
	git checkout master &&
	test_might_fail git branch -D branch &&
	git checkout -b branch &&
	touch exe &&
	chmod a+x exe &&
	git add exe &&
	git commit -m "exe" &&
	git checkout master &&
	git fs-cache-dump -vg &&
	git checkout branch &&
	git fs-cache-dump -vg
'

test_expect_success 'consistent fs_cache after checking out a soft directory link' '
	git checkout master &&
	(git branch -D branch || true) &&
	git checkout -b branch &&
	mkdir a &&
	ln -s a b &&
	git add a b &&
	git commit -m "directory link" &&
	git checkout master &&
	git fs-cache-dump -vg &&
	git checkout branch &&
	git fs-cache-dump -vg
'

test_expect_success 'consistent fs_cache after checking out a broken link' '
	git checkout master &&
	(git branch -D branch || true) &&
	git checkout -b branch &&
	touch a &&
	ln -s a b &&
	git add b &&
	git commit -m "broken link" &&
	git checkout master &&
	git fs-cache-dump -vg &&
	git checkout branch &&
	git fs-cache-dump -vg
'

test_expect_success 'fs_cache up-to-date after a clean' '
	touch foo bar baz &&
	git clean -dfx &&
	git status > /dev/null &&
	git fs-cache-dump -vg
'

test_expect_success 'file is up-to-date after a checkout' '
	git checkout master &&
	(git branch -D branch1 || true) &&
	git checkout -b branch1 &&
	touch vizzo &&
	git add vizzo &&
	git commit -m "files" &&
	git fs-cache-dump -vg &&
	git checkout master &&
	git fs-cache-dump -vg > output &&
	! grep "^vizzo" output &&
	git checkout branch1 &&
	git fs-cache-dump -vg > output &&
	grep "^vizzo" output
'

test_expect_success 'directory is up-to-date after a checkout' '
	git checkout master &&
	(git branch -D branch2 || true) &&
	git checkout -b branch2 &&
	mkdir -p vise &&
	touch vise/yuz &&
	git add vise/yuz &&
	git commit -m "new directory" &&
	git fs-cache-dump -vg &&
	git checkout master &&
	git fs-cache-dump -vg > output &&
	! grep "^vise/yuz" output &&
	git checkout branch2 &&
	git fs-cache-dump -vg > output &&
	grep "^vise/yuz" output
'

test_expect_success 'files are up to date after a merge' '
	git checkout master &&
	(git branch -D branch3 || true) &&
	git checkout -b branch3 &&
	touch yoss &&
	git add yoss &&
	git commit -m "added file to other branch"
	git merge branch1 &&
	git fs-cache-dump -vg > output &&
	grep "^vizzo" output &&
	grep "^yoss" output
'

test_expect_success 'dirs are up to date after a merge' '
	git checkout master &&
	(git branch -D branch4 || true) &&
	git checkout -b branch4 &&
	mkdir -p xox &&
	touch xox/hup &&
	git add xox/hup &&
	git commit -m "added dir to other branch"
	git merge branch2 &&
	git fs-cache-dump -vg > output &&
	grep "^xox/hup" output &&
	grep "^vise/yuz" output
'

test_expect_success 'fs_cache consistency after stash save operation' '
	touch fritzy &&
	mkdir ardvark &&
	touch ardvark/b &&
	touch ardvark/c &&
	ln -s ardvark/b bork &&
	git stash save -u &&
	git status > /dev/null &&
	git fs-cache-dump -vg > output &&
	! grep ardvark output
'

test_expect_success 'fs_cache consistency after stash pop operation' '
	git stash pop &&
	git fs-cache-dump -vg > output &&
	grep ardvark output
'

test_expect_success 'fs_cache reflects hard reset' '
	git add -A &&
	git reset --hard master &&
	git fs-cache-dump -vg > output &&
	! grep ardvark output
'

test_expect_success 'fs_cache tracks git mv' '
	touch cramniz &&
	mkdir -p stuffins &&
	git add -A &&
	git status >/dev/null &&
	git mv cramniz stuffins/booya &&
	git status >/dev/null &&
	git fs-cache-dump -vg > output &&
	! grep cramniz output &&
	grep stuffins/booya output
'

test_expect_success 'fs_cache tracks git rm' '
	touch brez &&
	mkdir -p gethype &&
	touch gethype/lotso &&
	touch gethype/more &&
	git add -A &&
	git status >/dev/null &&
	git rm -f brez &&
	git rm -r -f gethype &&
	git status >/dev/null &&
	git fs-cache-dump -vg > output &&
	! grep brez output &&
	! grep lotso output
'

test_expect_success 'fs_cache is up-to-date after a rebase' '
	git checkout master &&
	(git branch -D branch || true) &&
	git checkout -b branch &&
	touch tofoo tobar tobaz &&
	git add -A &&
	git commit -m "add some files" &&
	git checkout master &&
	(git branch -D branch2 || true) &&
	git checkout -b branch2 &&
	touch myso byso bay &&
	git add -A &&
	git commit -m "files elsewhere" &&
	git rebase --onto branch HEAD~1 &&
	git status >/dev/null &&
	git fs-cache-dump -vg > output &&
	grep tobaz output &&
	grep myso output
'

test_done
