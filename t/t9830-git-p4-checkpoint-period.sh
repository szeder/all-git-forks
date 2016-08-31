#!/bin/sh

test_description='git p4 checkpoint-period tests'

. ./lib-git-p4.sh

p4_submit_each () {
	for file in $@
	do
		echo $file > "$file" &&
		p4 add "$file" &&
		p4 submit -d "$file"
	done
}

test_expect_success 'start p4d' '
	start_p4d
'

test_expect_success 'no explicit checkpoints' '
	cd "$cli" &&
	p4_submit_each file1 file2 file3 &&
	git p4 clone --dest="$git" //depot@all &&
	test_when_finished cleanup_git &&
	(
		git -C "$git" reflog refs/remotes/p4/master >lines &&
		test_line_count = 1 lines &&
		p4_submit_each file4 file5 file6 &&
		git -C "$git" p4 sync &&
		git -C "$git" reflog refs/remotes/p4/master >lines &&
		test_line_count = 2 lines
	)
'

test_expect_success 'restart p4d' '
	kill_p4d &&
	start_p4d
'

test_expect_success 'checkpoint every 0 seconds, i.e. every commit' '
	cd "$cli" &&
	p4_submit_each file1 file2 file3 &&
	git p4 clone --dest="$git" --checkpoint-period 0 //depot@all &&
	test_when_finished cleanup_git &&
	(
		git -C "$git" reflog refs/remotes/p4/master >lines &&
		test_line_count = 3 lines &&
		p4_submit_each file4 file5 file6 &&
		git -C "$git" p4 sync --checkpoint-period 0 &&
		git -C "$git" reflog refs/remotes/p4/master >lines &&
		test_line_count = 6 lines
	)
'

test_expect_success 'kill p4d' '
	kill_p4d
'

test_done
