#!/bin/sh

test_description='combined diff filtering is not affected by preliminary path filtering'
# Since diff --cc allows use not only real parents but any commits, use merge
# base here as the 3rd "parent". The trick was suggested in $gmane/191557 to
# spot changes which were discarded during conflict resolution.

. ./test-lib.sh

test_expect_success setup '
	mkdir short &&
	mkdir long &&
	for fn in win1 win2 merge delete base only1 only2
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
	sed -e "s/^7/7change1/" long/only2 >sed.new &&
	mv sed.new long/only2 &&
	git add long/only2 &&
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
	sed -e "s/^11/11change2/" long/only1 >sed.new &&
	mv sed.new long/only1 &&
	git add long/only1 &&
	git commit -m branch2 &&
	git branch branch2 &&

	test_must_fail git merge branch1 &&
	git checkout mergebase -- . &&
	test_seq 11 | sed -e "s/^7/7change1/" -e "s/^11/11change2/" >long/base &&
	git add long/base &&
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

test_expect_success 'diff with mergebase shows discarded change from parent 2 in merged file' '
	git diff --cc merge branch1 branch2 mergebase -- long/win1 >actual &&
	test -s actual
'

test_expect_success 'diff with mergebase shows discarded change from parent 1 in merged file' '
	git diff --cc merge branch1 branch2 mergebase -- long/win2 >actual &&
	test -s actual
'

test_expect_failure 'diff with mergebase shows fully discarded file from parent 2' '
	git diff --cc merge branch1 branch2 mergebase -- short/win1 >actual &&
	test -s actual
'

test_expect_failure 'diff with mergebase shows fully discarded file from parent 1' '
	git diff --cc merge branch1 branch2 mergebase -- short/win2 >actual &&
	test -s actual
'

test_done
