#!/bin/sh

test_description='combined diff filtering is not affected by preliminary path filtering'
# Since diff --cc allows use not only real parents but any commits, use merge
# base here as the 3rd "parent". The trick was suggested in $gmane/191557 to
# spot changes which were discarded during conflict resolution.

. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

grep_line2() {
	grep -q \
	     -e '^[ +-][ +-][ +-]2$' \
	     -e '^[ +-][ +-][ +-]2change[12]$' \
	     -e '^[ +-][ +-][ +-]2merged$'
}

# history is:
# (mergebase) --> (branch1) --\
#  |                          V
#  \ --> (branch2) ----------(merge)
# there are files in 2 subdirectories, "long" and "short"
# each file in "long" subdirecty has exactly same history as same file in "short" one,
# but it has added lines with changes in both branches which merge without conflict
# so the long files are always selected at path filtering
test_expect_success setup '
	mkdir short &&
	mkdir long &&
	for fn in win1 win2 merge delete base only1 only2 only1discard only2discard
	do
		test_seq 3 >short/$fn &&
		git add short/$fn &&
		test_seq 11 >long/$fn &&
		git add long/$fn || return $?
	done &&
	git commit -m mergebase &&
	git branch mergebase &&

	for fn in win1 win2 merge delete base only1
	do
		for dir in short long
		do
			sed -e "s/^2/2change1/" -e "s/^7/7change1/" $dir/$fn >sed.new &&
			mv sed.new $dir/$fn &&
			git add $dir/$fn || return $?
		done || return $?
	done &&
	for fn in only2 only2discard
	do
	    sed -e "s/^7/7change1/" long/$fn >sed.new &&
	    mv sed.new long/$fn &&
	    git add long/$fn || return $?
	done &&
	git commit -m branch1 &&
	git branch branch1 &&

	git reset --hard mergebase &&
	for fn in win1 win2 merge delete base only2
	do
		for dir in short long
		do
			sed -e "s/^2/2change2/" -e "s/^11/11change2/" $dir/$fn >sed.new &&
			mv sed.new $dir/$fn &&
			git add $dir/$fn || return $?
		done || return $?
	done &&
	for fn in only1 only1discard
	do
	    sed -e "s/^11/11change2/" long/$fn >sed.new &&
	    mv sed.new long/$fn &&
	    git add long/$fn || return $?
	done &&
	git commit -m branch2 &&
	git branch branch2 &&

	test_must_fail git merge branch1 &&
	git checkout mergebase -- . &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" >long/base &&
	git add long/base &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" >long/only1discard &&
	git add long/only1discard &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" >long/only2discard &&
	git add long/only2discard &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "s/^2/2change1/" >long/win1 &&
	git add long/win1 &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "s/^2/2change2/" >long/win2 &&
	git add long/win2 &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "s/^2/2merged/" >long/merge &&
	git add long/merge &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "/^2/d" >long/delete &&
	git add long/delete &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "s/^2/2change1/" >long/only1 &&
	git add long/only1 &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" -e "s/^2/2change2/" >long/only2 &&
	git add long/only2 &&
	test_seq 3 >short/base &&
	git add short/base &&
	test_seq 3 >short/only1discard &&
	git add short/only1discard &&
	test_seq 3 >short/only2discard &&
	git add short/only2discard &&
	test_seq 3 | sed -e "s/^2/2change1/" >short/win1 &&
	git add short/win1 &&
	test_seq 3 | sed -e "s/^2/2change2/" >short/win2 &&
	git add short/win2 &&
	test_seq 3 | sed -e "s/^2/2merged/" >short/merge &&
	git add short/merge &&
	test_seq 3 | sed -e "/^2/d" >short/delete &&
	git add short/delete &&
	test_seq 3 | sed -e "s/^2/2change1/" >short/only1 &&
	git add short/only1 &&
	test_seq 3 | sed -e "s/^2/2change2/" >short/only2 &&
	git add short/only2 &&
	git commit -m merge &&
	git branch merge
'

# the difference in short file must be returned if and only if it is shown in long file
for fn in win1 win2 merge delete base only1 only2 only1discard only2discard
do
	if git diff --cc merge branch1 branch2 mergebase -- long/$fn | grep_line2
	then
		test_expect_success "diff --cc contains short/$fn" '
			git diff --cc merge branch1 branch2 mergebase -- short/'"$fn"' >actual &&
			test -s actual
		'
	else
		test_expect_success "diff --cc does not contain short/$fn" '
			git diff --cc merge branch1 branch2 mergebase -- short/'"$fn"' >actual &&
			! test -s actual
		'
	fi
done

for fn in win1 win2 merge delete base only1 only2 only1discard only2discard
do
	if git diff -c merge branch1 branch2 mergebase -- long/$fn | grep_line2
	then
		test_expect_success "diff -c contains short/$fn" '
			git diff -c merge branch1 branch2 mergebase -- short/'"$fn"' >actual &&
			test -s actual
		'
	else
		test_expect_success "diff -c does not contain short/$fn" '
			git diff -c merge branch1 branch2 mergebase -- short/'"$fn"' >actual &&
			! test -s actual
		'
	fi
done

test_done
