#!/bin/sh

test_description='basic tests for transfering external ODBs'

. ./test-lib.sh

ORIG_SOURCE="$PWD/.git"
export ORIG_SOURCE
ALT_SOURCE="$PWD/alt-repo/.git"
export ALT_SOURCE
write_script odb-helper <<\EOF
die() {
	printf >&2 "%s\n" "$@"
	exit 1
}
GIT_DIR=$ALT_SOURCE; export GIT_DIR
case "$1" in
have)
	git cat-file --batch-check --batch-all-objects |
	awk '{print $1 " " $3 " " $2}'
	;;
get)
	cat "$GIT_DIR"/objects/$(echo $2 | sed 's#..#&/#')
	;;
put)
	sha1="$2"
	size="$3"
	kind="$4"
	writen=$(git hash-object -w -t "$kind" --stdin)
	test "$writen" = "$sha1" || die "bad sha1 passed '$sha1' vs writen '$writen'"
	ref_hash=$(echo "$sha1 $size $kind" | GIT_DIR=$ORIG_SOURCE GIT_NO_EXTERNAL_ODB=1 git hash-object -w -t blob --stdin) || exit
	GIT_DIR=$ORIG_SOURCE git update-ref refs/odbs/magic/"$sha1" "$ref_hash"
	;;
*)
	die "unknown command '$1'"
	;;
esac
EOF
HELPER="\"$PWD\"/odb-helper"

test_expect_success 'setup alternate repo' '
	git init alt-repo &&
	git config odb.magic.command "$HELPER"
'

test_expect_success 'new blobs are put in object store' '
	test_commit one &&
	hash1=$(git ls-tree HEAD | grep one.t | cut -f1 | cut -d\  -f3) &&
	content=$(cd alt-repo && git show "$hash1") &&
	test "$content" = "one" &&
	test_commit two &&
	hash2=$(git ls-tree HEAD | grep two.t | cut -f1 | cut -d\  -f3) &&
	content=$(cd alt-repo && git show "$hash2") &&
	test "$content" = "two"
'

test_expect_success 'clone repo gets the objects' '
	git init other-repo &&
	(cd other-repo &&
	 git remote add origin .. &&
	 git fetch origin "refs/odbs/magic/*:refs/odbs/magic/*"
	) &&
	echo ok
'

test_done
