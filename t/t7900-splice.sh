#!/bin/sh
#
# Copyright (c) 2016 Adam Spiers
#

test_description='git splice

This tests all features of git-splice.
'

. ./test-lib.sh

TMP_BRANCH=tmp/splice

#############################################################################
# Setup

for i in one two three; do
	for j in a b; do
		tag=$i-$j
		test_expect_success "setup $i" "
			echo $i $j >> $i &&
			git add $i &&
			git commit -m \"$i $j\" &&
			git tag $tag"
	done
done
git_dir=`git rev-parse --git-dir`
latest_tag=$tag

test_expect_success "setup other branch" '
	git checkout -b four one-b &&
	for i in a b c; do
		echo four $i >> four &&
		git add four &&
		git commit -m "four $i" &&
		git tag "four-$i"
	done &&
	git checkout master
'

test_debug 'git show-ref'

del_tmp_branch () {
	git update-ref -d refs/heads/$TMP_BRANCH
}

reset () {
	# First check that tests don't leave a splice in progress,
	# as they should always do --abort or --continue if necessary
	test_splice_not_in_progress &&
	git reset --hard $latest_tag &&
	del_tmp_branch &&
	rm -f stdout stderr
}

test_splice_in_progress () {
	git splice --in-progress
}

head_ref ()
{
	git symbolic-ref --short -q HEAD
}

on_branch ()
{
	if [ "`head_ref`" = "$1" ]; then
		return 0
	else
		echo "not on $1 branch" >&2
		return 1
	fi
}

test_splice_not_in_progress () {
	test_must_fail git splice --in-progress &&
	test_must_fail git splice --continue 2>stderr &&
	    grep "Splice not in progress" stderr &&
	    test_debug 'echo "--continue failed as expected - good"' &&
	test_must_fail git splice --abort    2>stderr &&
	    grep "Splice not in progress" stderr &&
	    test_debug 'echo "--abort failed as expected - good"'
}

#############################################################################
# Invalid arguments

test_expect_success 'empty command line' '
	test_must_fail git splice 2>stderr &&
	grep "You must specify at least one range to remove or insert" stderr
'

test_expect_success 'too many arguments' '
	test_must_fail git splice a b c 2>stderr &&
	grep "Use of multiple words in the removal or insertion ranges requires the -- separator" stderr
'

test_only_one_option () {
	test_splice_not_in_progress &&
	test_must_fail git splice "$@" 2>stderr &&
	grep "You must only select one of --abort, --continue, and --in-progress" stderr &&
	test_splice_not_in_progress
}

for combo in \
	'--abort --continue' \
	'--continue --abort' \
	'--abort --in-progress' \
	'--in-progress --abort' \
	'--continue --in-progress' \
	'--in-progress --continue'
do
	test_expect_success "$combo" "
		test_only_one_option $combo
	"
done

test_expect_success 'insertion point without insertion range' '
	test_must_fail git splice one 2>stderr &&
	grep "You provided an insertion point but no range to cherry-pick" stderr &&
	test_splice_not_in_progress
'

test_failed_to_parse_removal_spec () {
	test_must_fail git splice "$@" 2>stderr &&
	grep "Failed to parse $*; aborting" stderr &&
	test_splice_not_in_progress
}

test_expect_success 'remove invalid single commit' '
	test_failed_to_parse_removal_spec five
'

test_expect_success 'remove range with invalid start' '
	test_failed_to_parse_removal_spec five..two-b
'

test_expect_success 'remove range with invalid end' '
	test_failed_to_parse_removal_spec two-b..five
'

#############################################################################
# Removing a single commit

test_remove_two_b () {
	reset &&
	git splice two-b^! "$@" &&
	grep "one b"   one   &&
	grep "three b" three &&
	grep "two a"   two   &&
	! grep "two b" two   &&
	test_splice_not_in_progress
}

test_expect_success 'remove single commit' '
	test_remove_two_b
'

test_expect_success 'remove single commit with --' '
	test_remove_two_b --
'

test_expect_success 'remove single commit causing conflict; abort' '
	reset &&
	test_must_fail git splice two-a^! >stdout 2>stderr &&
	grep "Could not apply .* two b" stderr &&
	grep "git rebase -i .* failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\"" stderr &&
	test_splice_in_progress &&
	git splice --abort &&
	test_splice_not_in_progress
'

test_expect_success 'remove single commit causing conflict; fix; continue' '
	reset &&
	test_must_fail git splice two-a^! >stdout 2>stderr &&
	grep "Could not apply .* two b" stderr &&
	grep "git rebase -i .* failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\"" stderr &&
	test_splice_in_progress &&
	echo two merged >two &&
	git add two &&
	git splice --continue &&
	grep "two merged" two &&
	grep "three b" three &&
	test_splice_not_in_progress
'

#############################################################################
# Invalid initial state

test_expect_success "checkout $TMP_BRANCH; ensure splice won't start" "
	test_when_finished 'git checkout master; del_tmp_branch' &&
	reset &&
	git checkout -b $TMP_BRANCH &&
	test_must_fail git splice two-b^! >stdout 2>stderr &&
	grep 'BUG: on $TMP_BRANCH branch, but no splice in progress' stderr &&
	del_tmp_branch &&
	test_splice_not_in_progress
"

test_expect_success "create $TMP_BRANCH; ensure splice won't start" "
	test_when_finished 'del_tmp_branch' &&
	reset &&
	git branch $TMP_BRANCH master &&
	test_must_fail git splice two-b^! >stdout 2>stderr &&
	grep 'BUG: $TMP_BRANCH branch exists, but no splice in progress' stderr &&
	del_tmp_branch &&
	test_splice_not_in_progress
"

test_expect_success "start cherry-pick with conflicts; ensure splice won't start" '
	test_when_finished "git cherry-pick --abort" &&
	reset &&
	test_must_fail git cherry-pick four-b >stdout 2>stderr &&
	grep "error: could not apply .* four b" stderr &&
	test_must_fail git splice two-b^! >stdout 2>stderr &&
	grep "Can'\''t start git splice when there is a cherry-pick in progress" stderr &&
	test_splice_not_in_progress
'

test_expect_success "start rebase with conflicts; ensure splice won't start" '
	test_when_finished "git rebase --abort" &&
	reset &&
	test_must_fail git rebase --onto one-b two-a >stdout 2>stderr &&
	grep "CONFLICT" stdout &&
	grep "Failed to merge in the changes" stderr &&
	test_must_fail git splice two-b^! >stdout 2>stderr &&
	grep "Can'\''t start git splice when there is a rebase in progress" stderr &&
	test_splice_not_in_progress
'

test_expect_success 'cause conflict; ensure not re-entrant' '
	test_when_finished "
		git splice --abort &&
		test_splice_not_in_progress
	" &&
	reset &&
	test_must_fail git splice two-a^! &&
	test_splice_in_progress &&
	test_must_fail git splice two-a^! >stdout 2>stderr &&
	grep "git splice already in progress; please complete it, or run" stderr &&
	grep "git splice --abort" stderr &&
	test_splice_in_progress
'

#############################################################################
# Removing a range of commits

test_remove_range_of_commits () {
	reset &&
	git splice one-b..two-b &&
	grep "one b"   one   &&
	grep "three b" three &&
	! test -e two           &&
	test_splice_not_in_progress
}

test_expect_success 'remove range of commits' '
	test_remove_range_of_commits
'

test_expect_success 'remove range of commits with --' '
	test_remove_range_of_commits --
'

test_expect_failure 'remove range of commits starting at root' '
	reset &&
	test_must_fail git splice one-b >stdout 2>stderr &&
	test_when_finished "
		git splice --abort &&
		test_splice_not_in_progress
	" &&
	! [ -e one ] &&
	test_splice_not_in_progress
'

test_expect_failure 'remove root commit' '
	reset &&
	test_must_fail git splice one-a^! >stdout 2>stderr &&
	test_when_finished "
		git splice --abort &&
		test_splice_not_in_progress
	" &&
	! [ -e one ] &&
	test_splice_not_in_progress
'

test_expect_failure 'dirty working tree prevents removing range' '
	reset &&
	echo dirty >>two &&
	test_when_finished "
		git splice --abort &&
		test_splice_not_in_progress
	" &&
	test_must_fail git splice two-b^! >stdout 2>stderr &&
	! grep rebase stderr &&
	test_splice_not_in_progress
'

test_expect_failure "dirty working tree doesn't prevent removing range" '
	reset &&
	echo dirty >>three &&
	test_when_finished "
		git splice --abort &&
		test_splice_not_in_progress
	" &&
	git splice two-b^! &&
	grep dirty three &&
	grep two-a two &&
	! grep two-b two &&
	grep three-b three &&
	test_splice_not_in_progress
'

#############################################################################
# Inserting a single commit

test_expect_success 'insert single commit' '
	reset &&
	git splice two-b four-a^! &&
	grep "two b" two &&
	grep "three a" three &&
	grep "four a" four &&
	! grep "four b" four &&
	git log --format=format:%s, | xargs |
		grep "three b, three a, four a, two b," &&
	test_splice_not_in_progress
'


#############################################################################
# Inserting a range of commits

test_expect_success 'insert commit range' '
	reset &&
	git splice two-b one-b..four-b &&
	grep "two b" two &&
	grep "three a" three &&
	grep "four b" four &&
	git log --format=format:%s, | xargs |
		grep "three b, three a, four b, four a, two b," &&
	test_splice_not_in_progress
'

test_expect_success 'insert commit causing conflict; abort' '
	reset &&
	test_must_fail git splice two-b four-b^! >stdout 2>stderr &&
	grep "could not apply .* four b" stderr &&
	grep "git cherry-pick failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\"" stderr &&
	test_splice_in_progress &&
	git splice --abort &&
	test_splice_not_in_progress
'

test_expect_success 'insert commit causing conflict; fix; continue' '
	reset &&
	test_must_fail git splice two-b four-b^! >stdout 2>stderr &&
	grep "could not apply .* four b" stderr &&
	grep "git cherry-pick failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\"" stderr &&
	test_splice_in_progress &&
	echo four merged >four &&
	git add four &&
	git splice --continue &&
	grep "four merged" four &&
	grep "three b" three &&
	test_splice_not_in_progress
'


#############################################################################
# Removing a range and inserting one or more commits

test_expect_success 'remove range; insert commit' '
	reset &&
	git splice two-a^..two-b four-a^! &&
	grep "four a" four &&
	! grep "four b" four &&
	grep "three b" three &&
	! [ -e two ] &&
	test_splice_not_in_progress
'

test_expect_success 'remove range; insert commit range' '
	reset &&
	git splice two-a^..two-b four-a^..four-b &&
	grep "four b" four &&
	! grep "four c" four &&
	grep "three b" three &&
	! [ -e two ] &&
	test_splice_not_in_progress
'

test_expect_success 'remove range; insert commit causing conflict; abort' '
	reset &&
	test_must_fail git splice two-a^..two-b four-b^! >stdout 2>stderr &&
	grep "could not apply .* four b" stderr &&
	grep "git cherry-pick failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\" to abandon the splice" stderr &&
	test_splice_in_progress &&
	git splice --abort &&
	test_splice_not_in_progress
'

test_remove_range_insert_commit_fix_conflict_continue ()
{
	reset &&
	test_must_fail git splice two-a^..two-b "$@" four-b^! >stdout 2>stderr &&
	grep "could not apply .* four b" stderr &&
	grep "git cherry-pick failed" stderr &&
	grep "When you have resolved this problem, run \"git splice --continue\"" stderr &&
	grep "or run \"git splice --abort\"" stderr &&
	test_splice_in_progress &&
	echo four merged >four &&
	git add four &&
	git splice --continue &&
	grep "four merged" four &&
	grep "three b" three &&
	! [ -e two ] &&
	test_splice_not_in_progress
}

test_expect_success 'remove range; insert commit causing conflict; fix; continue' '
	test_remove_range_insert_commit_fix_conflict_continue
'

test_expect_success 'remove range -- insert commit causing conflict; fix; continue' '
	test_remove_range_insert_commit_fix_conflict_continue --
'

test_expect_success 'remove grepped commits; insert grepped commits' '
	reset &&
	git splice --grep=two -n1 three-b -- --grep=four --skip=1 four &&
	grep "two a" two &&
	! grep "two b" two &&
	grep "four b" four &&
	! grep "four c" four &&
	grep "three b" three &&
	test_splice_not_in_progress
'

test_done
