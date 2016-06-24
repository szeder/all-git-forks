#!/bin/sh

test_description='basic tests for transfering external ODBs'

. ./test-lib.sh

ORIG_SOURCE="$PWD/.git"
export ORIG_SOURCE

ALT_SOURCE1="$PWD/alt-repo/.git"
export ALT_SOURCE1
write_script odb-helper1 <<\EOF
die() {
	printf >&2 "%s\n" "$@"
	exit 1
}
GIT_DIR=$ALT_SOURCE1; export GIT_DIR
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
HELPER1="\"$PWD\"/odb-helper1"

ALT_SOURCE2="$PWD/alt-repo/.git"
export ALT_SOURCE2
write_script odb-helper2 <<\EOF
die() {
	printf >&2 "%s\n" "$@"
	exit 1
}
GIT_DIR=$ALT_SOURCE2; export GIT_DIR
case "$1" in
have)
	git for-each-ref --format='%(objectname)' refs/odbs/magic/ | xargs git show
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
HELPER2="\"$PWD\"/odb-helper2"

test_expect_success 'setup first alternate repo' '
	git init alt-repo1 &&
	git config odb.magic.command "$HELPER1"
'

test_expect_success 'new blobs are put in object store' '
	test_commit one &&
	hash1=$(git ls-tree HEAD | grep one.t | cut -f1 | cut -d\  -f3) &&
	content=$(cd alt-repo1 && git show "$hash1") &&
	test "$content" = "one" &&
	test_commit two &&
	hash2=$(git ls-tree HEAD | grep two.t | cut -f1 | cut -d\  -f3) &&
	content=$(cd alt-repo1 && git show "$hash2") &&
	test "$content" = "two"
'

test_expect_success 'setup other repo and its alternate repo' '
	git init other-repo &&
	git init alt-repo1
'

test_expect_success 'other repo gets the objects' '
	(cd other-repo &&
	 git remote add origin .. &&
	 git fetch origin "refs/odbs/magic/*:refs/odbs/magic/*" &&
	 test_must_fail git cat-file blob "$hash1" &&
	 test_must_fail git cat-file blob "$hash2" &&
	 git config odb.magic.command "$HELPER2" &&
	 git cat-file blob "$hash1" &&
	 git cat-file blob "$hash2"
	)
'

test_done
