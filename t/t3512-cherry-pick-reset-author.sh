#!/bin/sh

test_description='git cherry-pick --reset-author'

. ./test-lib.sh

author_header () {
	git cat-file commit "$1" |
	sed -n -e '/^$/q' -e '/^author /p'
}

test_expect_success 'setup' '
	test_commit Initial &&
	test_commit Commit &&
	test_commit Some-change Commit &&
	git checkout -b Conflicting-change Commit &&
	test_commit Conflicting-change Commit
'

test_expect_success '--reset-author is incompatible with --ff' '
	git checkout Initial &&
	test_must_fail git cherry-pick --ff --reset-author Commit
'

test_expect_success '--reset-author is incompatible with --continue' '
	git checkout Some-change &&
	test_when_finished git cherry-pick --abort &&
	test_must_fail git cherry-pick --reset-author Conflicting-change &&
	test_must_fail git cherry-pick --continue --reset-author Commit
'

test_expect_success '--reset-author is incompatible with --quit' '
	git checkout Some-change &&
	test_when_finished git cherry-pick --abort &&
	test_must_fail git cherry-pick --reset-author Conflicting-change &&
	test_must_fail git cherry-pick --quit --reset-author Commit
'

test_expect_success '--reset-author is incompatible with --abort' '
	git checkout Some-change &&
	test_when_finished git cherry-pick --abort &&
	test_must_fail git cherry-pick --reset-author Conflicting-change &&
	test_must_fail git cherry-pick --abort --reset-author Commit
'

test_expect_success '--reset-author renews authorship' '
	git checkout -b cherry-pick-reset-author Commit &&
	test_tick &&
	git cherry-pick --reset-author Some-change &&
	echo "author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE" >expected.author &&
	author_header HEAD >actual.author &&
	test_cmp expected.author actual.author
'

test_expect_success '--reset-author renews authorship (interrupted)' '
	git checkout -b cherry-pick-reset-author-interrupted Some-change &&
	test_tick &&
	test_must_fail git cherry-pick --reset-author Conflicting-change &&
	git checkout --theirs -- Commit &&
	git add Commit &&
	git cherry-pick --continue &&
	echo "author $GIT_AUTHOR_NAME <$GIT_AUTHOR_EMAIL> $GIT_AUTHOR_DATE" >expected.author &&
	author_header HEAD >actual.author &&
	test_cmp expected.author actual.author
'
