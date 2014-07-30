#!/bin/sh

test_description='combined diff'

. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

setup_helper () {
	one=$1 branch=$2 side=$3 &&

	git branch $side $branch &&
	for l in $one two three fyra
	do
		echo $l
	done >file &&
	git add file &&
	test_tick &&
	git commit -m $branch &&
	git checkout $side &&
	for l in $one two three quatro
	do
		echo $l
	done >file &&
	git add file &&
	test_tick &&
	git commit -m $side &&
	test_must_fail git merge $branch &&
	for l in $one three four
	do
		echo $l
	done >file &&
	git add file &&
	test_tick &&
	git commit -m "merge $branch into $side"
}

verify_helper () {
	it=$1 &&

	# Ignore lines that were removed only from the other parent
	sed -e '
		1,/^@@@/d
		/^ -/d
		s/^\(.\)./\1/
	' "$it" >"$it.actual.1" &&
	sed -e '
		1,/^@@@/d
		/^- /d
		s/^.\(.\)/\1/
	' "$it" >"$it.actual.2" &&

	git diff "$it^" "$it" -- | sed -e '1,/^@@/d' >"$it.expect.1" &&
	test_cmp "$it.expect.1" "$it.actual.1" &&

	git diff "$it^2" "$it" -- | sed -e '1,/^@@/d' >"$it.expect.2" &&
	test_cmp "$it.expect.2" "$it.actual.2"
}

test_expect_success setup '
	>file &&
	git add file &&
	test_tick &&
	git commit -m initial &&

	git branch withone &&
	git branch sansone &&

	git checkout withone &&
	setup_helper one withone sidewithone &&

	git checkout sansone &&
	setup_helper "" sansone sidesansone
'

test_expect_success 'check combined output (1)' '
	git show sidewithone -- >sidewithone &&
	verify_helper sidewithone
'

test_expect_success 'check combined output (2)' '
	git show sidesansone -- >sidesansone &&
	verify_helper sidesansone
'

test_expect_success 'diagnose truncated file' '
	>file &&
	git add file &&
	git commit --amend -C HEAD &&
	git show >out &&
	grep "diff --cc file" out
'

test_expect_success 'setup for --cc --raw' '
	blob=$(echo file | git hash-object --stdin -w) &&
	base_tree=$(echo "100644 blob $blob	file" | git mktree) &&
	trees= &&
	for i in $(test_seq 1 40)
	do
		blob=$(echo file$i | git hash-object --stdin -w) &&
		trees="$trees$(echo "100644 blob $blob	file" | git mktree)$LF"
	done
'

test_expect_success 'check --cc --raw with four trees' '
	four_trees=$(echo "$trees" | sed -e 4q) &&
	git diff --cc --raw $four_trees $base_tree >out &&
	# Check for four leading colons in the output:
	grep "^::::[^:]" out
'

test_expect_success 'check --cc --raw with forty trees' '
	git diff --cc --raw $trees $base_tree >out &&
	# Check for forty leading colons in the output:
	grep "^::::::::::::::::::::::::::::::::::::::::[^:]" out
'

test_expect_success 'setup combined ignore spaces' '
	git checkout master &&
	>test &&
	git add test &&
	git commit -m initial &&

	tr -d Q <<-\EOF >test &&
	always coalesce
	eol space coalesce Q
	space  change coalesce
	all spa ces coalesce
	eol spaces Q
	space  change
	all spa ces
	EOF
	git commit -m "test space change" -a &&

	git checkout -b side HEAD^ &&
	tr -d Q <<-\EOF >test &&
	always coalesce
	eol space coalesce
	space change coalesce
	all spaces coalesce
	eol spaces
	space change
	all spaces
	EOF
	git commit -m "test other space changes" -a &&

	test_must_fail git merge master &&
	tr -d Q <<-\EOF >test &&
	eol spaces Q
	space  change
	all spa ces
	EOF
	git commit -m merged -a
'

test_expect_success 'check combined output (no ignore space)' '
	git show >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	--always coalesce
	- eol space coalesce
	- space change coalesce
	- all spaces coalesce
	- eol spaces
	- space change
	- all spaces
	 -eol space coalesce Q
	 -space  change coalesce
	 -all spa ces coalesce
	+ eol spaces Q
	+ space  change
	+ all spa ces
	EOF
	compare_diff_patch expected actual
'

test_expect_success 'check combined output (ignore space at eol)' '
	git show --ignore-space-at-eol >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	--always coalesce
	--eol space coalesce
	- space change coalesce
	- all spaces coalesce
	 -space  change coalesce
	 -all spa ces coalesce
	  eol spaces Q
	- space change
	- all spaces
	+ space  change
	+ all spa ces
	EOF
	compare_diff_patch expected actual
'

test_expect_success 'check combined output (ignore space change)' '
	git show -b >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	--always coalesce
	--eol space coalesce
	--space change coalesce
	- all spaces coalesce
	 -all spa ces coalesce
	  eol spaces Q
	  space  change
	- all spaces
	+ all spa ces
	EOF
	compare_diff_patch expected actual
'

test_expect_success 'check combined output (ignore all spaces)' '
	git show -w >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	--always coalesce
	--eol space coalesce
	--space change coalesce
	--all spaces coalesce
	  eol spaces Q
	  space  change
	  all spa ces
	EOF
	compare_diff_patch expected actual
'

test_expect_success 'combine diff coalesce simple' '
	>test &&
	git add test &&
	git commit -m initial &&
	test_seq 4 >test &&
	git commit -a -m empty1 &&
	git branch side1 &&
	git checkout HEAD^ &&
	test_seq 5 >test &&
	git commit -a -m empty2 &&
	test_must_fail git merge side1 &&
	>test &&
	git commit -a -m merge &&
	git show >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	--1
	--2
	--3
	--4
	- 5
	EOF
	compare_diff_patch expected actual
'

test_expect_success 'combine diff coalesce tricky' '
	>test &&
	git add test &&
	git commit -m initial --allow-empty &&
	cat <<-\EOF >test &&
	3
	1
	2
	3
	4
	EOF
	git commit -a -m empty1 &&
	git branch -f side1 &&
	git checkout HEAD^ &&
	cat <<-\EOF >test &&
	1
	3
	5
	4
	EOF
	git commit -a -m empty2 &&
	git branch -f side2 &&
	test_must_fail git merge side1 &&
	>test &&
	git commit -a -m merge &&
	git show >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	 -3
	--1
	 -2
	--3
	- 5
	--4
	EOF
	compare_diff_patch expected actual &&
	git checkout -f side1 &&
	test_must_fail git merge side2 &&
	>test &&
	git commit -a -m merge &&
	git show >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	- 3
	--1
	- 2
	--3
	 -5
	--4
	EOF
	compare_diff_patch expected actual
'

test_expect_failure 'combine diff coalesce three parents' '
	>test &&
	git add test &&
	git commit -m initial --allow-empty &&
	cat <<-\EOF >test &&
	3
	1
	2
	3
	4
	EOF
	git commit -a -m empty1 &&
	git checkout -B side1 &&
	git checkout HEAD^ &&
	cat <<-\EOF >test &&
	1
	3
	7
	5
	4
	EOF
	git commit -a -m empty2 &&
	git branch -f side2 &&
	git checkout HEAD^ &&
	cat <<-\EOF >test &&
	3
	1
	6
	5
	4
	EOF
	git commit -a -m empty3 &&
	>test &&
	git add test &&
	TREE=$(git write-tree) &&
	COMMIT=$(git commit-tree -p HEAD -p side1 -p side2 -m merge $TREE) &&
	git show $COMMIT >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	-- 3
	---1
	-  6
	 - 2
	 --3
	  -7
	- -5
	---4
	EOF
	compare_diff_patch expected actual
'

# Test for a bug reported at
# http://thread.gmane.org/gmane.comp.version-control.git/224410
# where a delete lines were missing from combined diff output when they
# occurred exactly before the context lines of a later change.
test_expect_success 'combine diff missing delete bug' '
	git commit -m initial --allow-empty &&
	cat <<-\EOF >test &&
	1
	2
	3
	4
	EOF
	git add test &&
	git commit -a -m side1 &&
	git checkout -B side1 &&
	git checkout HEAD^ &&
	cat <<-\EOF >test &&
	0
	1
	2
	3
	4modified
	EOF
	git add test &&
	git commit -m side2 &&
	git branch -f side2 &&
	test_must_fail git merge --no-commit side1 &&
	cat <<-\EOF >test &&
	1
	2
	3
	4modified
	EOF
	git add test &&
	git commit -a -m merge &&
	git diff-tree -c -p HEAD >actual.tmp &&
	sed -e "1,/^@@@/d" < actual.tmp >actual &&
	tr -d Q <<-\EOF >expected &&
	- 0
	  1
	  2
	  3
	 -4
	 +4modified
	EOF
	compare_diff_patch expected actual
'

# make a commit with contents "$1" and "$2" in the paths
# "one" and "two" respectively, and parents $3, $4, etc
#
# If the content is of the form "mode:content", then the
# mode for the given file is set (otherwise it defaults 100644).
mkcommit() {
	(
		GIT_INDEX_FILE=tmp.index &&
		export GIT_INDEX_FILE &&
		for path in one two; do
			case "$1" in
			*:*)
				mode=$(echo "$1" | cut -d: -f1)
				content=$(echo "$1" | cut -d: -f2)
				;;
			*)
				mode=100644
				content=$1
			esac
			blob=$(echo "$content" | git hash-object -w --stdin) &&
			git update-index --add --cacheinfo $mode,$blob,$path &&
			shift || exit 1
		done
		tree=$(git write-tree) &&
		parents=$(for p in "$@"; do echo "-p $p"; done) &&
		git commit-tree $tree $parents </dev/null
	)
}

# we create a merge commit where path "one" can be simplified, but
# path "two" cannot
test_expect_success 'simplify combined --raw' '
	side1=$(mkcommit base content1) &&
	side2=$(mkcommit base content2) &&
	merge=$(mkcommit new new $side1 $side2) &&
	cat >expect <<-\EOF &&
	:100644 100644 df967b9... 3e75765... M	one
	::100644 100644 100644 ac3e272... 637f034... 3e75765... MM	two
	EOF

	git diff-tree -c --simplify-combined-diff \
		--abbrev --format= $merge >actual &&
	test_cmp expect actual
'

# we do not use compare_diff_patch here because we want to
# make sure we get the headers right, too
test_expect_success 'simplify combined --patch' '
	side1=$(mkcommit base content1) &&
	side2=$(mkcommit base content2) &&
	merge=$(mkcommit new new $side1 $side2) &&

	cat >expect <<-\EOF &&
	diff --git a/one b/one
	index df967b9..3e75765 100644
	--- a/one
	+++ b/one
	@@ -1 +1 @@
	-base
	+new
	diff --combined two
	index ac3e272,637f034..3e75765
	--- a/two
	+++ b/two
	@@@ -1,1 -1,1 +1,1 @@@
	- content1
	 -content2
	++new
	EOF

	git diff-tree -c --simplify-combined-diff -p \
		--format= $merge >actual &&
	test_cmp expect actual
'

test_expect_success 'do not simplify unless all parents are identical' '
	side1=$(mkcommit base content1) &&
	test_tick &&
	side2=$(mkcommit base content1) &&
	side3=$(mkcommit base content3) &&
	merge=$(mkcommit new new $side1 $side2 $side3) &&

	cat >expect <<-\EOF &&
	diff --git a/one b/one
	index df967b9..3e75765 100644
	--- a/one
	+++ b/one
	@@ -1 +1 @@
	-base
	+new
	diff --combined two
	index ac3e272,ac3e272,27d10cc..3e75765
	--- a/two
	+++ b/two
	@@@@ -1,1 -1,1 -1,1 +1,1 @@@@
	-- content1
	  -content3
	+++new
	EOF

	git diff-tree -c --simplify-combined-diff -p \
		--format= $merge >actual &&
	test_cmp expect actual
'

test_expect_success 'do not simplify away mode changes' '
	side1=$(mkcommit 100644:base 100644:base) &&
	side2=$(mkcommit 100644:base 100755:base) &&
	merge=$(mkcommit new new $side1 $side2) &&

	cat >expect <<-\EOF &&
	diff --git a/one b/one
	index df967b9..3e75765 100644
	--- a/one
	+++ b/one
	@@ -1 +1 @@
	-base
	+new
	diff --combined two
	index df967b9,df967b9..3e75765
	mode 100644,100755..100644
	--- a/two
	+++ b/two
	@@@ -1,1 -1,1 +1,1 @@@
	--base
	++new
	EOF

	git diff-tree -c --simplify-combined-diff -p \
		--format= $merge >actual &&
	test_cmp expect actual
'

test_done
