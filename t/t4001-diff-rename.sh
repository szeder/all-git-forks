#!/bin/sh
#
# Copyright (c) 2005 Junio C Hamano
#

test_description='Test rename detection in diff engine.

'
. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

echo >path0 'Line 1
Line 2
Line 3
Line 4
Line 5
Line 6
Line 7
Line 8
Line 9
Line 10
line 11
Line 12
Line 13
Line 14
Line 15
'

test_expect_success \
    'update-index --add a file.' \
    'git update-index --add path0'

test_expect_success \
    'write that tree.' \
    'tree=$(git write-tree) && echo $tree'

sed -e 's/line/Line/' <path0 >path1
rm -f path0
test_expect_success \
    'renamed and edited the file.' \
    'git update-index --add --remove path0 path1'

test_expect_success \
    'git diff-index -p -M after rename and editing.' \
    'git diff-index -p -M $tree >current'
cat >expected <<\EOF
diff --git a/path0 b/path1
rename from path0
rename to path1
--- a/path0
+++ b/path1
@@ -8,7 +8,7 @@ Line 7
 Line 8
 Line 9
 Line 10
-line 11
+Line 11
 Line 12
 Line 13
 Line 14
EOF

test_expect_success \
    'validate the output.' \
    'compare_diff_patch current expected'

test_expect_success 'favour same basenames over different ones' '
	cp path1 another-path &&
	git add another-path &&
	git commit -m 1 &&
	git rm path1 &&
	mkdir subdir &&
	git mv another-path subdir/path1 &&
	git status | test_i18ngrep "renamed: .*path1 -> subdir/path1"'

test_expect_success 'favour same basenames even with minor differences' '
	git show HEAD:path1 | sed "s/15/16/" > subdir/path1 &&
	git status | test_i18ngrep "renamed: .*path1 -> subdir/path1"'

test_expect_success 'setup for many rename source candidates' '
	git reset --hard &&
	for i in 0 1 2 3 4 5 6 7 8 9;
	do
		for j in 0 1 2 3 4 5 6 7 8 9;
		do
			echo "$i$j" >"path$i$j"
		done
	done &&
	git add "path??" &&
	test_tick &&
	git commit -m "hundred" &&
	(cat path1; echo new) >new-path &&
	echo old >>path1 &&
	git add new-path path1 &&
	git diff -l 4 -C -C --cached --name-status >actual 2>actual.err &&
	sed -e "s/^\([CM]\)[0-9]*	/\1	/" actual >actual.munged &&
	cat >expect <<-EOF &&
	C	path1	new-path
	M	path1
	EOF
	test_cmp expect actual.munged &&
	grep warning actual.err
'

test_expect_success 'rename pretty print with nothing in common' '
	mkdir -p a/b/ &&
	: >a/b/c &&
	git add a/b/c &&
	git commit -m "create a/b/c" &&
	mkdir -p c/b/ &&
	git mv a/b/c c/b/a &&
	git commit -m "a/b/c -> c/b/a" &&
	git diff -M --summary HEAD^ HEAD >output &&
	test_i18ngrep " a/b/c => c/b/a " output &&
	git diff -M --stat HEAD^ HEAD >output &&
	test_i18ngrep " a/b/c => c/b/a " output
'

test_expect_success 'rename pretty print with common prefix' '
	mkdir -p c/d &&
	git mv c/b/a c/d/e &&
	git commit -m "c/b/a -> c/d/e" &&
	git diff -M --summary HEAD^ HEAD >output &&
	test_i18ngrep " c/{b/a => d/e} " output &&
	git diff -M --stat HEAD^ HEAD >output &&
	test_i18ngrep " c/{b/a => d/e} " output
'

test_expect_success 'rename pretty print with common suffix' '
	mkdir d &&
	git mv c/d/e d/e &&
	git commit -m "c/d/e -> d/e" &&
	git diff -M --summary HEAD^ HEAD >output &&
	test_i18ngrep " {c/d => d}/e " output &&
	git diff -M --stat HEAD^ HEAD >output &&
	test_i18ngrep " {c/d => d}/e " output
'

test_expect_success 'rename pretty print with common prefix and suffix' '
	mkdir d/f &&
	git mv d/e d/f/e &&
	git commit -m "d/e -> d/f/e" &&
	git diff -M --summary HEAD^ HEAD >output &&
	test_i18ngrep " d/{ => f}/e " output &&
	git diff -M --stat HEAD^ HEAD >output &&
	test_i18ngrep " d/{ => f}/e " output
'

test_expect_success 'rename pretty print common prefix and suffix overlap' '
	mkdir d/f/f &&
	git mv d/f/e d/f/f/e &&
	git commit -m "d/f/e d/f/f/e" &&
	git diff -M --summary HEAD^ HEAD >output &&
	test_i18ngrep " d/f/{ => f}/e " output &&
	git diff -M --stat HEAD^ HEAD >output &&
	test_i18ngrep " d/f/{ => f}/e " output
'

test_expect_success 'manual rename correction' '
	test_create_repo correct-rename &&
	(
		cd correct-rename &&
		echo one > old-one &&
		echo two > old-two &&
		git add old-one old-two &&
		git commit -m old &&
		git rm old-one old-two &&
		echo one > new-one &&
		echo two > new-two &&
		git add new-one new-two &&
		git commit -m new &&
		git diff -M --summary HEAD^ | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-one => new-one (100%)
		 rename old-two => new-two (100%)
		EOF
		test_cmp expected actual &&

		cat >correction <<-\EOF &&
		old-one => new-two
		old-two => new-one
		EOF
		git diff -M --rename-file=correction --summary HEAD^ | sort | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-one => new-two (100%)
		 rename old-two => new-one (100%)
		EOF
		test_cmp expected actual
	)
'

test_expect_success 'manual rename correction with blobs' '
	(
		cd correct-rename &&
		git diff -M --summary HEAD^ | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-one => new-one (100%)
		 rename old-two => new-two (100%)
		EOF
		test_cmp expected actual &&

		ONE=`echo one | git hash-object --stdin` &&
		TWO=`echo two | git hash-object --stdin` &&
		cat >correction <<-EOF &&
		.blob $ONE => $TWO
		.blob $TWO => $ONE
		EOF
		git diff -M --rename-file=correction --summary HEAD^ | sort | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-one => new-two (100%)
		 rename old-two => new-one (100%)
		EOF
		test_cmp expected actual
	)
'

test_expect_success 'rename correction from notes' '
	(
		cd correct-rename &&
		git show --summary -M HEAD | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-one => new-one (100%)
		 rename old-two => new-two (100%)
		EOF
		test_cmp expected actual &&

		cat >correction <<-\EOF &&
		old-one => new-two
		old-two => new-one
		EOF
		git notes --ref=rename add -F correction HEAD &&
		git show --summary -M --rename-notes=rename HEAD | grep rename >actual &&
		cat >expected <<-\EOF &&
		 rename old-two => new-one (100%)
		 rename old-one => new-two (100%)
		EOF
		test_cmp expected actual
	)
'

test_expect_success 'merge rename notes, free src/tgt' '
	(
		cd correct-rename &&
		test-merge-rename-notes refs/notes/rename refs/notes/rename-cache &&
		git notes --ref=rename-cache show refs/notes/rename >actual &&
		: >expected &&
		test_cmp expected actual &&
		ONE=`echo one | git hash-object --stdin` &&
		TWO=`echo two | git hash-object --stdin` &&
		git notes --ref=rename-cache show $TWO >actual &&
		cat <<-EOF | sort >expected &&
		.blob $ONE => $TWO
		EOF
		test_cmp expected actual &&
		git notes --ref=rename-cache show $ONE >actual &&
		cat <<-EOF | sort >expected &&
		.blob $TWO => $ONE
		EOF
		test_cmp expected actual
	)
'

test_done
