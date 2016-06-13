#!/bin/sh
#
# Copyright (c) 2016 Adam Spiers
#

test_description='git transplant

This tests all features of git-transplant.
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
orig_master=$( git rev-parse HEAD )

del_tmp_branch () {
	git update-ref -d refs/heads/$TMP_BRANCH &&
	echo "deleted $TMP_BRANCH"
}

reset () {
	# First check that tests don't leave a transplant in progress,
	# as they should always do --abort or --continue if necessary.
	# We also expect them to leave the master branch checked out.
	test_transplant_not_in_progress &&
	on_branch master &&
	git reset --hard $latest_tag &&
	git branch -f four four-c &&
	git update-ref -d refs/heads/new &&
	rm -f stdout stderr
}

test_transplant_in_progress () {
	git transplant --in-progress
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

valid_ref ()
{
	if git rev-parse --quiet --verify "$1" >/dev/null; then
		echo "ref $1 exists"
		return 0
	else
		echo "ref $1 doesn't exist"
		return 1
	fi
}

refs_equal ()
{
	a=$( git rev-parse "$1" )
	b=$( git rev-parse "$2" )
	if [ "$a" = "$b" ]; then
		echo "$1 is same commit as $2"
		return 0
	else
		echo "$1 is different commit to $2 ($a vs. $b)"
		return 1
	fi
}

on_original_master ()
{
	on_branch master &&
	refs_equal master "$orig_master"
}

test_transplant_not_in_progress () {
	test_must_fail git transplant --in-progress &&
	test_must_fail git transplant --continue 2>stderr &&
	    grep -q "Transplant not in progress" stderr &&
	    test_debug 'echo "--continue failed as expected - good"' &&
	test_must_fail git transplant --abort    2>stderr &&
	    grep -q "Transplant not in progress" stderr &&
	    test_debug 'echo "--abort failed as expected - good"'
}

#############################################################################
# Invalid arguments

test_expect_success 'empty command line' '
	test_must_fail git transplant 2>stderr &&
	cat stderr &&
	grep "Incorrect number of arguments" stderr
'

test_expect_success 'only one argument' '
	test_must_fail git transplant foo 2>stderr &&
	grep "Incorrect number of arguments" stderr
'

test_expect_success 'too many arguments' '
	test_must_fail git transplant a b c 2>stderr &&
	grep "Incorrect number of arguments" stderr
'

test_expect_success 'invalid start of commit range' '
	test_must_fail git transplant a..two-b four 2>stderr &&
	grep "Failed to parse a..two-b" stderr
'

test_expect_success 'invalid end of commit range' '
	test_must_fail git transplant two-a^..five four 2>stderr &&
	grep "Failed to parse two-a^..five" stderr
'

test_expect_success 'single commitish instead of transplant range' '
	test_must_fail git transplant two-a four 2>stderr &&
	grep "TRANSPLANT_RANGE must not be a reference to a single commit" stderr
'

test_expect_success 'invalid destination branch' '
	test_must_fail git transplant two-a^..two-b blah 2>stderr &&
	grep "Failed to parse blah" stderr
'

test_expect_success "destination wasn't a branch" '
	test_must_fail git transplant two-a^..two-b four^ 2>stderr &&
	grep "Destination four^ isn'\''t a branch" stderr
'

for sep in ' ' '='; do
	test_expect_success "invalid --after${sep}ref" "
		test_must_fail git transplant \
			--after${sep}blah two-a^..two-b four 2>stderr &&
		grep 'Failed to parse blah' stderr
	"

	test_expect_success "invalid --new-from${sep}ref" "
		test_must_fail git transplant \
			--new-from${sep}blah two-a^..two-b new 2>stderr &&
		test_debug 'cat stderr' &&
		grep 'Failed to parse blah' stderr
	"

	test_expect_success "existing dest branch with --new-from${sep}ref" "
		test_must_fail git transplant \
			--new-from${sep}blah two-a^..two-b four 2>stderr &&
		test_debug 'cat stderr' &&
		grep 'four should not already exist when using --new-from' stderr
	"
done

test_only_one_option () {
	test_transplant_not_in_progress &&
	test_must_fail git transplant $1 2>stderr &&
	grep "You must only select one of $2" stderr &&
	test_transplant_not_in_progress
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
		test_only_one_option \"$combo\" \"--abort, --continue, and --in-progress\"
	"
done

for combo in \
	'--after foo --new-from bar' \
	'--new-from bar --after foo'
do
	test_expect_success "$combo" "
		test_only_one_option \"$combo\" \"--after and --new-from\"
	"
done

#############################################################################
# Valid transplants

transplant_two_to () {
	dest_branch="$1"
	shift
	reset &&
	echo git transplant "$@" two-a^..two-b "$dest_branch" &&
	git transplant "$@" two-a^..two-b "$dest_branch" &&
	git show ${dest_branch}:two | grep "two a" &&
	git show ${dest_branch}:two | grep "two b" &&
	! [ -e two ] &&
	test_transplant_not_in_progress &&
	test_master_unchanged
}

test_master_unchanged () {
	branch_history master |
		grep "three b, three a, one b, one a"
}

branch_history () {
	git log --format=format:%s, "$@" | xargs
}

test_expect_success 'transplant a range' '
	transplant_two_to four &&
	branch_history four | grep "two b, two a, four c,"
'

for sep in ' ' '='; do
	test_expect_success "transplant range inside branch (--after${sep}ref)" "
		transplant_two_to four --after${sep}four-a &&
		branch_history four |
			grep 'four c, four b, two b, two a, four a'
	"

	for from in four four-a; do
		from_arg="--new-from$sep$from"
		test="transplant range onto new branch ($from_arg)"
		new_history='two b, two a'
		case "$from" in
		    four)
			new_history="$new_history, four c, four b, four a"
			;;
		    four-a)
			new_history="$new_history, four a"
			;;
		esac
		test_expect_success "$test" "
			transplant_two_to new $from_arg &&
			branch_history four |
				grep 'four c, four b, four a' &&
			branch_history new |
				grep '$new_history'
		"
	done
done

#############################################################################
# Invalid initial state

test_expect_success "checkout $TMP_BRANCH; ensure transplant won't start" "
	test_when_finished 'git checkout master; del_tmp_branch' &&
	reset &&
	git checkout -b $TMP_BRANCH &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	grep 'BUG: $TMP_BRANCH branch exists, but no splice in progress' stderr &&
	git checkout master &&
	del_tmp_branch &&
	test_transplant_not_in_progress
"

test_expect_success "create $TMP_BRANCH; ensure transplant won't start" "
	test_when_finished 'del_tmp_branch' &&
	reset &&
	git branch $TMP_BRANCH master &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	grep 'BUG: $TMP_BRANCH branch exists, but no splice in progress' stderr &&
	on_original_master &&
	del_tmp_branch &&
	test_transplant_not_in_progress
"

test_expect_success "start cherry-pick with conflicts; ensure transplant won't start" '
	test_when_finished "git cherry-pick --abort" &&
	reset &&
	test_must_fail git cherry-pick four-b >stdout 2>stderr &&
	grep "error: could not apply .* four b" stderr &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	grep "Can'\''t start git transplant when there is a cherry-pick in progress" stderr &&
	on_original_master &&
	del_tmp_branch &&
	test_transplant_not_in_progress
'

test_expect_success "start rebase with conflicts; ensure transplant won't start" '
	test_when_finished "git rebase --abort" &&
	reset &&
	test_must_fail git rebase --onto one-b two-a >stdout 2>stderr &&
	grep "CONFLICT" stdout &&
	grep "Failed to merge in the changes" stderr &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	grep "Can'\''t start git transplant when there is a rebase in progress" stderr &&
	del_tmp_branch &&
	test_transplant_not_in_progress
'

test_expect_success 'cause conflict; ensure not re-entrant' '
	test_when_finished "
		git transplant --abort &&
		test_transplant_not_in_progress
	" &&
	reset &&
	test_must_fail git transplant two-a^! four &&
	test_transplant_in_progress &&
	test_must_fail git transplant two-a^! four >stdout 2>stderr &&
	grep "git transplant already in progress; please complete it, or run" stderr &&
	grep "git transplant --abort" stderr &&
	test_transplant_in_progress
'

test_expect_failure 'dirty working tree prevents removing range' '
	reset &&
	echo dirty >>two &&
	test_when_finished "
		test_transplant_not_in_progress
	" &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	! grep rebase stderr &&
	test_transplant_not_in_progress
'

test_expect_failure "dirty working tree doesn't prevent removing range" '
	reset &&
	echo dirty >>three &&
	test_when_finished "
		git transplant --abort &&
		test_transplant_not_in_progress
	" &&
	git transplant two-b^! four &&
	grep dirty three &&
	grep two-a two &&
	! grep two-b two &&
	grep three-b three &&
	test_transplant_not_in_progress
'

#############################################################################
# Handling conflicts

test_expect_success 'transplant commit causing insertion conflict; abort' '
	reset &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	test_debug "echo STDOUT; cat stdout; echo ----" &&
	test_debug "echo STDERR; cat stderr; echo ----" &&
	grep "CONFLICT.*: two deleted in HEAD and modified in .* two b" stdout &&
	grep "error: could not apply .* two b" stderr &&
	grep "When you have resolved this problem, run \"git transplant --continue\"" stderr &&
	grep "or run \"git transplant --abort\"" stderr &&
	git transplant --abort &&
	on_original_master &&
	refs_equal four four-c &&
	test_transplant_not_in_progress
'

test_expect_success 'transplant commit to new causing insertion conflict; abort' '
	reset &&
	test_must_fail git transplant -n four two-b^! four-two-b >stdout 2>stderr &&
	test_debug "echo STDOUT; cat stdout; echo ----" &&
	test_debug "echo STDERR; cat stderr; echo ----" &&
	grep "CONFLICT.*: two deleted in HEAD and modified in .* two b" stdout &&
	grep "error: could not apply .* two b" stderr &&
	grep "When you have resolved this problem, run \"git transplant --continue\"" stderr &&
	grep "or run \"git transplant --abort\"" stderr &&
	git transplant --abort &&
	on_original_master &&
	refs_equal four four-c &&
	! valid_ref four-two-b &&
	test_transplant_not_in_progress
'

test_expect_success 'transplant commit causing removal conflict; abort' '
	reset &&
	test_must_fail git transplant two-a^! four >stdout 2>stderr &&
	test_debug "echo STDOUT; cat stdout; echo ----" &&
	test_debug "echo STDERR; cat stderr; echo ----" &&
	grep "error: could not apply .* two b" stdout &&
	grep "CONFLICT .*: two deleted in HEAD and modified in .* two b" stdout &&
	grep "When you have resolved this problem, run \"git transplant --continue\"" stderr &&
	grep "or run \"git transplant --abort\"" stderr &&
	git transplant --abort &&
	on_original_master &&
	refs_equal four four-c &&
	test_transplant_not_in_progress
'

test_expect_success 'transplant commit causing insertion conflict; continue' '
	reset &&
	test_must_fail git transplant two-b^! four >stdout 2>stderr &&
	test_debug "echo STDOUT; cat stdout; echo ----" &&
	test_debug "echo STDERR; cat stderr; echo ----" &&
	grep "CONFLICT.*: two deleted in HEAD and modified in .* two b" stdout &&
	grep "error: could not apply .* two b" stderr &&
	grep "When you have resolved this problem, run \"git transplant --continue\"" stderr &&
	grep "or run \"git transplant --abort\"" stderr &&
	echo "two b resolved" > two &&
	git add two &&
	git transplant --continue &&
	test_transplant_not_in_progress &&
	! refs_equal four four-c &&
	git show four:two | grep "two b resolved" &&
	branch_history master |
		grep "three b, three a, two a, one b, one a" &&
	branch_history four |
		grep "two b, four c, four b, four a, one b, one a"
'

test_expect_success 'transplant commit causing removal conflict; continue' '
	reset &&
	test_must_fail git transplant two-a^! four >stdout 2>stderr &&
	test_debug "echo STDOUT; cat stdout; echo ----" &&
	test_debug "echo STDERR; cat stderr; echo ----" &&
	grep "error: could not apply .* two b" stdout &&
	grep "CONFLICT .*: two deleted in HEAD and modified in .* two b" stdout &&
	grep "When you have resolved this problem, run \"git transplant --continue\"" stderr &&
	grep "or run \"git transplant --abort\"" stderr &&
	echo "two b resolved" >two &&
	git add two &&
	git transplant --continue &&
	test_transplant_not_in_progress &&
	! refs_equal four four-c &&
	git show four:two | grep "two a" &&
	grep "two b resolved" two &&
	branch_history master |
		grep "three b, three a, two b, one b, one a" &&
	branch_history four |
		grep "two a, four c, four b, four a, one b, one a" &&
	test_transplant_not_in_progress
'

test_done
