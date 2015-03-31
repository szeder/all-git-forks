#!/bin/sh

test_description='credential-cache tests'
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-credential.sh

test -z "$NO_UNIX_SOCKETS" || {
	skip_all='skipping credential-cache tests, unix sockets not available'
	test_done
}

# don't leave a stale daemon running
trap 'code=$?; git credential-cache exit; unset XDG_RUNTIME_DIR; (exit $code); die' EXIT

helper_test cache
helper_test_timeout cache --timeout=1

test_expect_success 'use fallback path when XDG_RUNTIME_DIR not set' '
	test -S "$HOME"/.git-credential-cache/socket &&
	test_path_is_missing "$HOME"/run/git-credential-cache-socket
'

# we can't rely on our "trap" above working after test_done,
# as test_done will delete the trash directory containing
# our socket, leaving us with no way to access the daemon.
git credential-cache exit

test_expect_success 'setup XDG_RUNTIME_DIR' '
	XDG_RUNTIME_DIR="$HOME"/run &&
	export XDG_RUNTIME_DIR &&
	mkdir -m 700 "$XDG_RUNTIME_DIR"
'

helper_test cache
helper_test_timeout cache --timeout=1

test_expect_failure 'use XDG_RUNTIME_DIR when set' '
	test_path_is_missing "$HOME"/.git-credential-cache/socket &&
	test -S "$XDG_RUNTIME_DIR"/git-credential-cache-socket
'

git credential-cache exit

test_done
