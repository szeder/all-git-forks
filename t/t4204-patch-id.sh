#!/bin/sh

test_description='git patch-id'

. ./test-lib.sh

test_expect_success 'setup' '
	cat > a <<-\EOF &&
		a
		a
		a
		a
		a
		a
		a
		a
		EOF
	(cat a; echo b) > ab &&
	(echo d; cat a; echo b) > dab &&
	cp a foo &&
	cp a bar &&
	git add foo bar &&
	git commit -a -m initial &&
	cp ab foo &&
	cp ab bar &&
	git commit -a -m first &&
	git checkout -b same master &&
	git commit --amend -m same-msg &&
	git checkout -b notsame master &&
	echo c > foo &&
	echo c > bar &&
	git commit --amend -a -m notsame-msg &&
	git checkout -b split master &&
	cp dab foo &&
	cp dab bar &&
	git commit -a -m split &&
	git checkout -b merged master &&
	git checkout split -- foo bar &&
	git commit --amend -a -m merged &&
	cat > bar-then-foo <<-\EOF &&
		bar
		foo
		EOF
	cat > foo-then-bar <<-\EOF
		foo
		bar
		EOF
'

test_expect_success 'patch-id output is well-formed' '
	git log -p -1 | git patch-id > output &&
	grep "^[a-f0-9]\{40\} $(git rev-parse HEAD)$" output
'

calc_patch_id () {
	git patch-id |
		sed "s# .*##" > patch-id_"$1"
}

calc_patch_id_unstable () {
	git patch-id --unstable |
		sed "s# .*##" > patch-id_"$1"
}

calc_patch_id_stable () {
	git patch-id --stable |
		sed "s# .*##" > patch-id_"$1"
}


get_patch_id () {
	git log -p -1 "$1" -O bar-then-foo -- | git patch-id |
		sed "s# .*##" > patch-id_"$1"
}

get_patch_id_stable () {
	git log -p -1 "$1" -O bar-then-foo | git patch-id --stable |
		sed "s# .*##" > patch-id_"$1"
}

get_patch_id_unstable () {
	git log -p -1 "$1" -O bar-then-foo | git patch-id --unstable |
		sed "s# .*##" > patch-id_"$1"
}


test_expect_success 'patch-id detects equality' '
	get_patch_id master &&
	get_patch_id same &&
	test_cmp patch-id_master patch-id_same
'

test_expect_success 'patch-id detects inequality' '
	get_patch_id master &&
	get_patch_id notsame &&
	! test_cmp patch-id_master patch-id_notsame
'

test_expect_success 'patch-id supports git-format-patch output' '
	get_patch_id master &&
	git checkout same &&
	git format-patch -1 --stdout | calc_patch_id same &&
	test_cmp patch-id_master patch-id_same &&
	set `git format-patch -1 --stdout | git patch-id` &&
	test "$2" = `git rev-parse HEAD`
'

test_expect_success 'whitespace is irrelevant in footer' '
	get_patch_id master &&
	git checkout same &&
	git format-patch -1 --stdout | sed "s/ \$//" |
		calc_patch_id same &&
	test_cmp patch-id_master patch-id_same
'

test_expect_success 'file order is irrelevant by default' '
	get_patch_id master &&
	git checkout same &&
	git format-patch -1 --stdout -O foo-then-bar |
		calc_patch_id same &&
	test_cmp patch-id_master patch-id_same
'

test_expect_success 'file order is irrelevant with --stable' '
	get_patch_id_stable master &&
	git checkout same &&
	git format-patch -1 --stdout -O foo-then-bar |
		calc_patch_id_stable same &&
	test_cmp patch-id_master patch-id_same
'

test_expect_success 'file order is relevant with --unstable' '
	get_patch_id_unstable master &&
	git checkout same &&
	git format-patch -1 --stdout -O foo-then-bar | calc_patch_id_unstable notsame &&
	! test_cmp patch-id_master patch-id_notsame
'

test_expect_success 'splitting patch does not affect id by default' '
	get_patch_id merged &&
	(git log -p -1 -O foo-then-bar split~1; git diff split~1..split) |
		calc_patch_id split &&
	test_cmp patch-id_merged patch-id_split
'

test_expect_success 'splitting patch does not affect id with --stable' '
	get_patch_id_stable merged &&
	(git log -p -1 -O foo-then-bar split~1; git diff split~1..split) |
		calc_patch_id_stable split &&
	test_cmp patch-id_merged patch-id_split
'

test_expect_success 'splitting patch affects id with --unstable' '
	get_patch_id_unstable merged &&
	(git log -p -1 -O foo-then-bar split~1; git diff split~1..split) |
		calc_patch_id_unstable split &&
	! test_cmp patch-id_merged patch-id_split
'

test_expect_success 'patch-id supports git-format-patch MIME output' '
	get_patch_id master &&
	git checkout same &&
	git format-patch -1 --attach --stdout | calc_patch_id same &&
	test_cmp patch-id_master patch-id_same
'

cat >nonl <<\EOF
diff --git i/a w/a
index e69de29..2e65efe 100644
--- i/a
+++ w/a
@@ -0,0 +1 @@
+a
\ No newline at end of file
diff --git i/b w/b
index e69de29..6178079 100644
--- i/b
+++ w/b
@@ -0,0 +1 @@
+b
EOF

cat >withnl <<\EOF
diff --git i/a w/a
index e69de29..7898192 100644
--- i/a
+++ w/a
@@ -0,0 +1 @@
+a
diff --git i/b w/b
index e69de29..6178079 100644
--- i/b
+++ w/b
@@ -0,0 +1 @@
+b
EOF

test_expect_success 'patch-id handles no-nl-at-eof markers' '
	cat nonl | calc_patch_id nonl &&
	cat withnl | calc_patch_id withnl &&
	test_cmp patch-id_nonl patch-id_withnl
'
test_done
