#!/bin/sh

test_description='check output directory names used by git-clone'
. ./test-lib.sh

# we use a fake ssh wrapper that ignores the arguments
# entirely; we really only care that we get _some_ repo,
# as the real test is what clone does on the local side
test_expect_success 'setup ssh wrapper' '
	write_script "$TRASH_DIRECTORY/ssh-wrapper" <<-\EOF &&
	git upload-pack "$TRASH_DIRECTORY"
	EOF
	GIT_SSH="$TRASH_DIRECTORY/ssh-wrapper" &&
	export GIT_SSH &&
	export TRASH_DIRECTORY
'

# make sure that cloning $1 results in local directory $2
test_clone_dir () {
	url=$1; shift
	dir=$1; shift
	expect=success
	bare=non-bare
	clone_opts=
	for i in "$@"; do
		case "$i" in
		fail)
			expect=failure
			;;
		bare)
			bare=bare
			clone_opts=--bare
			;;
		esac
	done
	test_expect_$expect "clone of $url goes to $dir ($bare)" "
		rm -rf $dir &&
		git clone $clone_opts $url &&
		test_path_is_dir $dir
	"
}

# basic syntax with bare and non-bare variants
test_clone_dir host:foo foo
test_clone_dir host:foo foo.git bare
test_clone_dir host:foo.git foo
test_clone_dir host:foo.git foo.git bare
test_clone_dir host:foo/.git foo
test_clone_dir host:foo/.git foo.git bare

# similar, but using ssh URL rather than host:path syntax
test_clone_dir ssh://host/foo foo
test_clone_dir ssh://host/foo foo.git bare
test_clone_dir ssh://host/foo.git foo
test_clone_dir ssh://host/foo.git foo.git bare
test_clone_dir ssh://host/foo/.git foo
test_clone_dir ssh://host/foo/.git foo.git bare

# we should remove trailing slashes
test_clone_dir ssh://host/foo/ foo
test_clone_dir ssh://host/foo.git/ foo
test_clone_dir ssh://host/foo/.git/ foo

# omitting the path should default to the hostname
test_clone_dir ssh://host/ host
test_clone_dir ssh://host:1234/ host
test_clone_dir ssh://user@host/ host
test_clone_dir ssh://user:password@host/ host
test_clone_dir ssh://user:password@host:1234/ host

# trailing port-like numbers should not be stripped for paths
test_clone_dir ssh://user:password@host/test:1234 1234
test_clone_dir ssh://user:password@host/test:1234.git 1234

test_done
