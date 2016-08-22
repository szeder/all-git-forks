#!/bin/sh

test_description='Test diff indent heuristic.

'
. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

# Compare two diff outputs. Ignore "index" lines, because we don't
# care about SHA-1s or file modes.
compare_diff () {
	sed -e "/^index /d" <"$1" >.tmp-1
	sed -e "/^index /d" <"$2" >.tmp-2
	test_cmp .tmp-1 .tmp-2 && rm -f .tmp-1 .tmp-2
}

test_expect_success 'diff: favor trailing blank lines' '
	cat <<-\EOF >old &&
	1
	2
	a

	b
	3
	4
	EOF

	cat <<-\EOF >new &&
	1
	2
	a

	b
	a

	b
	3
	4
	EOF

	tr "_" " " <<-\EOF >expect &&
	diff --git a/old b/new
	--- a/old
	+++ b/new
	@@ -3,5 +3,8 @@
	 a
	_
	 b
	+a
	+
	+b
	 3
	 4
	EOF

	tr "_" " " <<-\EOF >expect-compacted &&
	diff --git a/old b/new
	--- a/old
	+++ b/new
	@@ -2,6 +2,9 @@
	 2
	 a
	_
	+b
	+a
	+
	 b
	 3
	 4
	EOF

	test_must_fail git diff --no-index old new >out &&
	compare_diff expect out &&

	test_must_fail git diff --no-index --indent-heuristic old new >out-compacted &&
	compare_diff expect-compacted out-compacted &&

	test_must_fail git -c diff.indentHeuristic=true diff --no-index old new >out-compacted2 &&
	compare_diff expect-compacted out-compacted2 &&

	test_must_fail git diff --indent-heuristic --patience --no-index old new >out-compacted3 &&
	compare_diff expect-compacted out-compacted3 &&

	test_must_fail git diff --indent-heuristic --histogram --no-index old new >out-compacted4 &&
	compare_diff expect-compacted out-compacted4
'

test_expect_success 'diff: keep functions together' '
	cat <<-\EOF >old &&
	1
	2
	/* function */
	foo() {
	    foo
	}

	3
	4
	EOF

	cat <<-\EOF >new &&
	1
	2
	/* function */
	bar() {
	    foo
	}

	/* function */
	foo() {
	    foo
	}

	3
	4
	EOF

	tr "_" " " <<-\EOF >expect &&
	diff --git a/old b/new
	--- a/old
	+++ b/new
	@@ -1,6 +1,11 @@
	 1
	 2
	 /* function */
	+bar() {
	+    foo
	+}
	+
	+/* function */
	 foo() {
	     foo
	 }
	EOF

	tr "_" " " <<-\EOF >expect-compacted &&
	diff --git a/old b/new
	--- a/old
	+++ b/new
	@@ -1,5 +1,10 @@
	 1
	 2
	+/* function */
	+bar() {
	+    foo
	+}
	+
	 /* function */
	 foo() {
	     foo
	EOF

	test_must_fail git diff --no-index old new >out &&
	compare_diff expect out &&

	test_must_fail git diff --no-index --indent-heuristic old new >out-compacted &&
	compare_diff expect-compacted out-compacted
'

test_done
