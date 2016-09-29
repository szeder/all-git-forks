#!/bin/sh

test_description='tests for external objects to an HTTPD server'

. ./test-lib.sh

# If we don't specify a port the current test number will be used
# which will not work as it is less than 1024, so it can only be used by root.
LIB_HTTPD_PORT=3440

. "$TEST_DIRECTORY"/lib-httpd.sh

start_httpd apache-e-odb.conf


#SMART=smart
#MYURL="$HTTPD_URL/$SMART/repo.git"
#MYURL="$HTTPD_URL/repo.git"
MYURL="$HTTPD_URL/upload/"

echo "MYURL: $MYURL"
echo "GIT_EXEC_PATH: $GIT_EXEC_PATH"

test_expect_success 'server request log matches test results' '
	curl --data "Hello Apache World" --include "$MYURL" >out
'

exit 1

stop_httpd

test_done

exit 0

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



stop_httpd

test_done
