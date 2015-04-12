#!/bin/sh

set -e

# allways,diverged,secondParent(,never -- the script not called)
merge_config="$1"
remote="$2"
remote_remote_ref="$3"
remote_local_ref="$4"

push_merge_head_file=$(git rev-parse --git-path PUSH_MERGE_HEAD)

tmpdir=$(mktemp -d)
if test -z "$tmpdir" || test "$tmpdir" = /
then
	echo "Invalid tmpdir" >/dev/stderr
	exit 1
fi
old_head=$(git rev-parse --symbolic-full-name HEAD)
if test "$old_head" = HEAD
then
	old_head=$(git rev-parse HEAD)
fi
trap 'git update-ref --no-deref HEAD "$old_head" && rm -rf "$tmpdir"' 0

# TODO: activate it with parameter; should be run only between push attempts
git fetch "$remote" "$remote_remote_ref:$remote_local_ref"

if git merge-base --is-ancestor HEAD "$remote_local_ref"
then
	# pushed already
	echo "pushed already" >/dev/stderr
	exit 0
fi

case "$merge_config"
in
diverged)
	if git merge-base --is-ancestor "$remote_local_ref" HEAD
	then
		echo "diverged: can ff" >/dev/stderr
		# can be pushed cleanly; no need to merge
		echo "$old_head" >"$push_merge_head_file"
		exit 0
	fi
	;;
always)
	;;
secondParent)
	echo "merge=secondParent: not implemented" >/dev/stderr
	exit 1
	;;
*)
	echo "unexpected merge: $merge" >/dev/stderr
	exit 1
	;;
esac

echo "push_merge: merging" >/dev/stderr
GIT_INDEX_FILE="$tmpdir/index"
export GIT_INDEX_FILE
git update-ref --no-deref HEAD "$remote_local_ref"'^'
git read-tree HEAD
# TODO: nice message
echo "push_merge: merging2" >/dev/stderr
git merge --log --edit -Xindex-only "$old_head"
git rev-parse HEAD "$push_merge_head_file"
