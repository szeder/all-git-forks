#!/bin/sh

set -e

# allways,diverged,secondParent(,never -- the script not called)
merge_config="$1"
remote="$2"
remote_remote_ref="$3"
remote_local_ref="$4"

push_merge_head_file=$(git rev-parse --git-path PUSH_MERGE_HEAD)
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
		# can be pushed cleanly; no need to merge
		git rev-parse HEAD >"$push_merge_head_file"
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

tmpdir=$(mktemp -d)
if test -z "$tmpdir" || test "$tmpdir" = /
then
	echo "Invalid tmpdir" >/dev/stderr
	exit 1
fi
trap 'rm -rf "$tmpdir"' 0
GIT_INDEX_FILE="$tmpdir/index"

git read-tree "$remote_local_ref"
git-merge-recursive --index-only $(git merge-base -a "$remote_local_ref" HEAD) -- "$remote_local_ref" HEAD
new_head=$(git write-tree | git-commit-tree)
echo "$new_head" >"$push_merge_head_file"
