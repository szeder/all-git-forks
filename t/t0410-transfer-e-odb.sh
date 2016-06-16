#!/bin/sh

test_description='basic tests for transfering external ODBs'

. ./test-lib.sh

OBJECT_STORE="$PWD/object-store/"
export OBJECT_STORE
write_script odb-helper <<\EOF
die() {
	printf >&2 "%s\n" "$@"
	exit 1
}
echo >&2 "Script called with args: $@"
case "$1" in
have)
	ls "$OBJECT_STORE"/ | tr - ' '
	;;
get)
	cat "$OBJECT_STORE"/"$sha1"*
	;;
put)
	sha1="$2"
	size="$3"
	kind="$4"
	cat >"$OBJECT_STORE"/"$sha1"-"$size"-"$kind" || exit
	ref_hash=$(echo "$sha1 $size $kind" | GIT_NO_EXTERNAL_ODB=1 git hash-object -w -t blob --stdin) || exit
	git update-ref refs/odbs/magic/"$sha1" "$ref_hash"
	;;
*)
	die "unknown command '$1'"
	;;
esac
EOF
HELPER="\"$PWD\"/odb-helper"

test_expect_success 'setup object store' '
	mkdir object-store &&
	git config odb.magic.command "$HELPER"
'

test_expect_success 'new blobs are put in object store' '
	test_commit one &&
	hash1=$(git ls-tree HEAD | grep one.t | cut -f1 | cut -d\  -f3) &&
	ls "$OBJECT_STORE"/"$hash1"-* &&
	test_commit two &&
	hash2=$(git ls-tree HEAD | grep two.t | cut -f1 | cut -d\  -f3) &&
	ls "$OBJECT_STORE"/"$hash2"-*
'

test_done
