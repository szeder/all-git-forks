#
# Library code for git-p4 tests
#

. ./test-lib.sh

if ! test_have_prereq PYTHON; then
	skip_all='skipping git-p4 tests; python not available'
	test_done
fi
( p4 -h && p4d -h ) >/dev/null 2>&1 || {
	skip_all='skipping git-p4 tests; no p4 or p4d'
	test_done
}

GITP4="$GIT_BUILD_DIR/contrib/fast-import/git-p4"

# Try to pick a unique port: guess a large number, then hope
# no more than one of each test is running.
#
# This does not handle the case where somebody else is running the
# same tests and has chosen the same ports.
testid=${this_test#t}
git_p4_test_start=9800
P4DPORT=$((10669 + ($testid - $git_p4_test_start)))

export P4PORT=localhost:$P4DPORT
export P4CLIENT=client

db="$TRASH_DIRECTORY/db"
cli="$TRASH_DIRECTORY/cli"
git="$TRASH_DIRECTORY/git"
pidfile="$TRASH_DIRECTORY/p4d.pid"

start_p4d() {
	mkdir -p "$db" "$cli" "$git" &&
	(
		p4d -q -r "$db" -p $P4DPORT &
		echo $! >"$pidfile"
	) &&
	for i in 1 2 3 4 5 ; do
		p4 info >/dev/null 2>&1 && break || true &&
		echo waiting for p4d to start &&
		sleep 1
	done &&
	# complain if it never started
	p4 info >/dev/null &&
	(
		cd "$cli" &&
		p4 client -i <<-EOF
		Client: client
		Description: client
		Root: $cli
		View: //depot/... //client/...
		EOF
	)
}

kill_p4d() {
	pid=$(cat "$pidfile")
	# it had better exist for the first kill
	kill $pid &&
	for i in 1 2 3 4 5 ; do
		kill $pid >/dev/null 2>&1 || break
		sleep 1
	done &&
	# complain if it would not die
	test_must_fail kill $pid >/dev/null 2>&1 &&
	rm -rf "$db" "$cli" "$pidfile"
}

cleanup_git() {
	rm -rf "$git"
}

#
# This is a handy tool when developing or debugging tests.  Use
# it inline to pause the script, perhaps like this:
#
#	"$GITP4" clone ... &&
#	(
#		cd "$git" &&
#		debug &&
#		git log --oneline >lines &&
#		...
#
# Go investigate when it pauses, then hit ctrl-c to continue the
# test.  The other tests will run, and p4d will be cleaned up nicely.
#
# Note that the directory is deleted and created for every test run,
# so you have to do the "cd" again.
#
# The continuation feature only works in shells that do not propagate
# a child-caught ctrl-c, namely bash.  With ash, the entire test run
# will exit on the ctrl-c.
#
debug() {
	echo "*** Debug me, hit ctrl-c when done.  Useful shell commands:"
	echo cd \"$(pwd)\"
	echo export P4PORT=$P4PORT P4CLIENT=$P4CLIENT
	trap "echo" INT
	sleep $((3600 * 24 * 30))
	trap - INT
}
