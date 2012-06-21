#!/bin/sh

test_description='object name disambiguation'
. ./test-lib.sh

test_expect_success setup '
	# Create a blob, a tree, a commit and a tag that
	# all share this prefix.  In addition, the tree,
	# the commit and the tag all share a single "a"
	# after this prefix, though the next hexadecimal
	# in their names are all different.
	prefix=110282 &&

	file=b6abx6 &&
	echo 6y3o >"$file" &&
	git add "$file" &&
	test_tick &&
	git commit -m 2tfnc &&
	git tag -m ry096b v1.0.0 &&
	echo "blob  " $(git rev-parse ":$file") &&
	echo "tree  " $(git rev-parse "HEAD^{tree}") &&
	echo "commit" $(git rev-parse "HEAD^{commit}") &&
	echo "tag   " $(git rev-parse "v1.0.0") &&
	describe=$(git describe --always --long HEAD) &&
	short_describe="v1.0.0-0-g$prefix"
'

test_expect_success 'can parse describe name' '
	# The name describe gives is always unambiguous
	echo "$describe" &&
	git rev-parse --verify "$describe"
'

test_expect_success 'cannot parse ambiguous prefix name' '
	# Without context to help disambiguate, $prefix alone
	# is ambiguous.
	echo "$prefix" &&
	test_must_fail git rev-parse --verify "$prefix"
'

test_expect_success 'can parse unambiguous and shorter describe name' '
	# There are other types of objects that share $prefix,
	# but there is only one commit.
	echo "$short_describe" &&
	git rev-parse --verify "$short_describe"
'

test_done
