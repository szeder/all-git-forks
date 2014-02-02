#!/bin/sh

test_description='File watcher daemon and client tests'

. ./test-lib.sh

if git file-watcher --check-support && test_have_prereq POSIXPERM; then
	: # good
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

GIT_TEST_WATCHER=2
GIT_TEST_WATCHER_PATH="$TRASH_DIRECTORY"
export GIT_TEST_WATCHER GIT_TEST_WATCHER_PATH

test_expect_success 'setup' '
	chmod 700 . &&
	mkdir foo bar &&
	touch abc foo/def bar/ghi &&
	git add . &&
	git file-watcher --detach . &&
	cat <<EOF >expect &&
<test-mode
>test mode on
EOF
	test-file-watcher . <expect >actual &&
	test_cmp expect actual
'

test_expect_success 'initial opening sequence' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git ls-files >/dev/null &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< inconsistent
packet:          git> watch 20
packet:          git< watched 3
EOF
	test_cmp expect actual &&

	# second time gives the same result because file-watch has not
	# received new-index
	GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual2" git ls-files >/dev/null &&
	test_cmp expect actual2
'

test_expect_success 'full sequence' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git status >/dev/null &&
	SIG2=`test-file-watcher --index-signature .git/index` &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< inconsistent
packet:          git> watch 20
packet:          git< watched 3
packet:          git> new-index $SIG2
packet:          git> unchange 0
EOF
	test_cmp expect actual
'

test_expect_success 'full sequence after file-watcher is active' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git ls-files -v >paths.actual &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< ok
packet:          git> get-changed
packet:          git< changed 0
EOF
	test_cmp expect actual &&
	cat <<EOF >paths.expect &&
w abc
w bar/ghi
w foo/def
EOF
	test_cmp paths.expect paths.actual
'

test_expect_success 'inject a file change' '
	echo modified >bar/ghi &&
	SIG=`test-file-watcher --index-signature .git/index` &&
	cat >expect <<EOF &&
<hello
>hello
<index $SIG $TRASH_DIRECTORY
>ok
<inotify 2 IN_MODIFY ghi
<dump changes
>>changed
>>bar/ghi
EOF
	test-file-watcher . >actual <expect &&
	test_cmp expect actual
'

test_expect_success 'git obtains the changes' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TEST_WATCHER=1 GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git ls-files -v >paths.actual &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< ok
packet:          git> get-changed
packet:          git< changed 8
EOF
	test_cmp expect actual &&
	cat <<EOF >paths.expect &&
w abc
H bar/ghi
w foo/def
EOF
	test_cmp paths.expect paths.actual
'

test_expect_success 'sync file-watcher after index update' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git status --porcelain | grep -vF "??" >paths.actual &&
	SIG2=`test-file-watcher --index-signature .git/index` &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< ok
packet:          git> get-changed
packet:          git< changed 8
packet:          git> watch 8
packet:          git< watched 1
packet:          git> new-index $SIG2
packet:          git> unchange 8
EOF
	test_cmp expect actual &&
	cat <<EOF >paths.expect &&
A  abc
AM bar/ghi
A  foo/def
EOF
	test_cmp paths.expect paths.actual
'

test_expect_success 'make sure file-watcher cleans its changed list' '
	SIG=`test-file-watcher --index-signature .git/index` &&
	rm actual &&
	GIT_TEST_WATCHER=1 GIT_TRACE_PACKET="$TRASH_DIRECTORY/actual" git ls-files -v >paths.actual &&
	cat <<EOF >expect &&
packet:          git> hello
packet:          git< hello
packet:          git> index $SIG $TRASH_DIRECTORY
packet:          git< ok
packet:          git> get-changed
packet:          git< changed 0
EOF
	test_cmp expect actual &&
	cat <<EOF >paths.expect &&
w abc
H bar/ghi
w foo/def
EOF
	test_cmp paths.expect paths.actual
'

test_expect_success 'closing the daemon' '
	test-file-watcher . <<EOF >/dev/null
<die
>see you
EOF
'

test_done
