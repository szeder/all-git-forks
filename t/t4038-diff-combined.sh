#!/bin/sh

test_description='combined diff'

. ./test-lib.sh

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
	for i in `test_seq 1 40`
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

	echo "
	always coalesce
	eol space coalesce \n\
	space  change coalesce
	all spa ces coalesce
	eol spaces \n\
	space  change
	all spa ces" >test &&
	git commit -m "change three" -a &&

	git checkout -b side HEAD^ &&
	echo "
	always coalesce
	eol space coalesce
	space change coalesce
	all spaces coalesce
	eol spaces
	space change
	all spaces" >test &&
	git commit -m indent -a &&

	test_must_fail git merge master &&
	echo "
	eol spaces \n\
	space  change
	all spa ces" > test &&
	git commit -m merged -a
'

test_expect_success 'check combined output (no ignore space)' '
	git show | test_i18ngrep "^-\s*eol spaces" &&
	git show | test_i18ngrep "^-\s*eol space coalesce" &&
	git show | test_i18ngrep "^-\s*space change" &&
	git show | test_i18ngrep "^-\s*space change coalesce" &&
	git show | test_i18ngrep "^-\s*all spaces" &&
	git show | test_i18ngrep "^-\s*all spaces coalesce" &&
	git show | test_i18ngrep "^--\s*always coalesce"
'

test_expect_success 'check combined output (ignore space at eol)' '
	git show --ignore-space-at-eol | test_i18ngrep "^\s*eol spaces" &&
	git show --ignore-space-at-eol | test_i18ngrep "^--\s*eol space coalesce" &&
	git show --ignore-space-at-eol | test_i18ngrep "^-\s*space change" &&
	git show --ignore-space-at-eol | test_i18ngrep "^-\s*space change coalesce" &&
	git show --ignore-space-at-eol | test_i18ngrep "^-\s*all spaces" &&
	git show --ignore-space-at-eol | test_i18ngrep "^-\s*all spaces coalesce" &&
	git show --ignore-space-at-eol | test_i18ngrep "^--\s*always coalesce"
'

test_expect_success 'check combined output (ignore space change)' '
	git show -b | test_i18ngrep "^\s*eol spaces" &&
	git show -b | test_i18ngrep "^--\s*eol space coalesce" &&
	git show -b | test_i18ngrep "^\s*space  change" &&
	git show -b | test_i18ngrep "^--\s*space change coalesce" &&
	git show -b | test_i18ngrep "^-\s*all spaces" &&
	git show -b | test_i18ngrep "^-\s*all spaces coalesce" &&
	git show -b | test_i18ngrep "^--\s*always coalesce"
'

test_expect_success 'check combined output (ignore all spaces)' '
	git show -w | test_i18ngrep "^\s*eol spaces" &&
	git show -w | test_i18ngrep "^--\s*eol space coalesce" &&
	git show -w | test_i18ngrep "^\s*space  change" &&
	git show -w | test_i18ngrep "^--\s*space change coalesce" &&
	git show -w | test_i18ngrep "^\s*all spa ces" &&
	git show -w | test_i18ngrep "^--\s*all spaces coalesce" &&
	git show -w | test_i18ngrep "^--\s*always coalesce"
'

test_done
