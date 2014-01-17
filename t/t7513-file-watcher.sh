#!/bin/sh

test_description='File watcher tests'

. ./test-lib.sh

if git file-watcher --check-support && test_have_prereq POSIXPERM; then
	:				# good
else
	skip_all="file-watcher not supported on this system"
	test_done
fi

kill_it() {
	test-file-watcher "$1" <<EOF >/dev/null
<die
>see you
EOF
}

GIT_TEST_WATCHER=1
export GIT_TEST_WATCHER

test_expect_success 'test-file-watcher can kill the daemon' '
	git file-watcher --detach . &&
	cat >expect <<EOF &&
<die
>see you
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	! test -S socket
'

test_expect_success 'exchange hello' '
	git file-watcher --detach . &&
	cat >expect <<EOF &&
<hello
>hello
<die
>see you
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'normal index sequence' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>inconsistent
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	cat >expect2 <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<die
>see you
EOF
	test-file-watcher . >actual2 <expect2 &&
	test_cmp expect2 actual2
'

test_expect_success 'unaccepted index: hello not sent' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<index $SIG $TRASH_DIRECTORY
>error why did you not greet me? go away
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	kill_it .
'

test_expect_success 'unaccepted index: signature too short' '
	git file-watcher --detach . &&
	cat >expect <<EOF &&
<hello
>hello
<index 1234 $TRASH_DIRECTORY
>error invalid index line index 1234 $TRASH_DIRECTORY
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	kill_it .
'

test_expect_success 'unaccepted index: worktree unavailable' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY/non-existent
>error work tree does not exist: No such file or directory
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	kill_it .
'

test_expect_success 'watch foo and abc/bar' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>inconsistent
<test-mode
<<watch
<<foo
<<abc/bar
<<
>watched 2
<dump-watches
>>watching
>>. 1
>>abc 2
>>abc/bar
>>foo
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'modify abc/bar' '
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<inotify 2 IN_MODIFY bar
<dump-watches
>>watching
>>. 1
>>foo
<dump-changes
>>changed
>>abc/bar
<die
>see you
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'delete abc makes abc/bar changed' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>inconsistent
<test-mode
<<watch
<<foo/abc/bar
<<
>watched 1
<dump-watches
>>watching
>>. 1
>>foo 2
>>foo/abc 3
>>foo/abc/bar
<inotify 2 IN_DELETE_SELF
<dump-watches
>>watching
<dump-changes
>>changed
>>foo/abc/bar
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'get changed list' '
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<get-changed
>>changed
>>foo/abc/bar
>>NULL
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'incomplete new-index request' '
	SIG=0123456789012345678901234567890123456789 &&
	SIG2=9123456789012345678901234567890123456780 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<new-index $SIG2
<<unchange
<<foo/abc/bar
<<
<dump-changes
>>changed
>>foo/abc/bar
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'delete abc/bar from changed list' '
	SIG=0123456789012345678901234567890123456789 &&
	SIG2=9123456789012345678901234567890123456780 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<new-index $SIG2
<<unchange
<<foo/abc/bar
<<NULL
<<
<dump-changes
>>changed
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'file-watcher index updated after new-index' '
	SIG2=9123456789012345678901234567890123456780 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG2 $TRASH_DIRECTORY
>ok
<die
>see you
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

# When test-mode is on, file-watch only accepts 8 directories
test_expect_success 'watch too many directories' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>inconsistent
# Do not call inotify_add_watch()
<test-mode
# First batch should be all ok
<<watch
<<dir1/foo
<<dir2/foo
<<dir3/foo
<<dir4/foo
<<
>watched 4
# Second batch hits the limit
<<watch
<<dir5/foo
<<dir6/foo
<<dir7/foo
<<dir8/foo
<<dir9/foo
<<
>watched 3
# The third batch is already registered, should accept too
<<watch
<<dir5/foo
<<dir6/foo
<<dir7/foo
<<
>watched 3
# Try again, see if it still rejects
<<watch
<<dir8/foo
<<dir9/foo
<<
>watched 0
<dump-watches
>>watching
>>. 1
>>dir1 2
>>dir1/foo
>>dir2 3
>>dir2/foo
>>dir3 4
>>dir3/foo
>>dir4 5
>>dir4/foo
>>dir5 6
>>dir5/foo
>>dir6 7
>>dir6/foo
>>dir7 8
>>dir7/foo
<die
>see you
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'event overflow' '
	git file-watcher --detach . &&
	SIG=0123456789012345678901234567890123456789 &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>inconsistent
<test-mode
<<watch
<<foo
<<abc/bar
<<
>watched 2
<inotify 2 IN_MODIFY bar
<dump-watches
>>watching
>>. 1
>>foo
<dump-changes
>>changed
>>abc/bar
<inotify -1 IN_Q_OVERFLOW
<dump-watches
>>watching
<dump-changes
>>changed
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual &&
	cat >expect2 <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
# Must be inconsistent because of IN_Q_OVERFLOW
>inconsistent
<die
>see you
EOF
	test-file-watcher . >actual2 <expect2 &&
	test_cmp expect2 actual2
'

test_done
