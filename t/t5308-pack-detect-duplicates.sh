#!/bin/sh

test_description='handling of duplicate objects in incoming packfiles'
. ./test-lib.sh

# git will never intentionally create packfiles with
# duplicate objects, so we have to construct them by hand.
#
# $1 is the number of times to duplicate each object (must be
# less than 127 for simplicify of implementation).
create_pack() {
	{
		# header magic
		printf 'PACK' &&
		# version 2
		printf '\0\0\0\2' &&
		# num of objects
		printf '\0\0\0\'"$(printf "%o" $(($1*2)))" &&

		for i in $(test_seq 1 "$1"); do
			# blob e69de29bb2d1d6434b8b29ae775ad8c2e48c5391
			printf '\060\170\234\003\0\0\0\0\1' &&
			# blob e68fe8129b546b101aee9510c5328e7f21ca1d18
			printf '\062\170\234\143\267\3\0\0\116\0\106'
		done
	} >tmp.pack &&
	# we store and cat the pack because we also have to output
	# the sha1 pack trailer
	cat tmp.pack &&
	test-sha1 <tmp.pack | hex2bytes &&
	rm -f tmp.pack
}

# convert an ascii hex representation of bytes into binary
hex2bytes() {
	"$PERL_PATH" -ne 's/[0-9a-f]{2}/print chr hex $&/ge'
}

# remove any existing packs, since we want to make
# sure future operations use any new packs we are about
# to install
clear_packs() {
	rm -f .git/objects/pack/*
}

# The sha1s we have in our pack. It's important that these have the same
# starting byte, so that they end up in the same fanout section of the index.
# That lets us make sure we are exercising the binary search with both sets.
LO_SHA1=e68fe8129b546b101aee9510c5328e7f21ca1d18
HI_SHA1=e69de29bb2d1d6434b8b29ae775ad8c2e48c5391

# And here's a "missing sha1" which will produce failed lookups. It must also
# be in the same fanout section, and should be between the two (so that during
# our binary search, we are sure to end up looking at one or the other of the
# duplicate runs).
MISSING_SHA1='e69d000000000000000000000000000000000000'

# double-check that create_pack actually works
test_expect_success 'pack with no duplicates' '
	create_pack 1 >no-dups.pack &&
	git index-pack --stdin <no-dups.pack
'

test_expect_success 'index-pack will allow duplicate objects by default' '
	clear_packs &&
	create_pack 100 >dups.pack &&
	git index-pack --stdin <dups.pack
'

test_expect_success 'create test vectors' '
	cat >input <<-EOF &&
	$LO_SHA1
	$HI_SHA1
	$MISSING_SHA1
	EOF
	cat >expect <<-EOF
	$LO_SHA1 blob 2
	$HI_SHA1 blob 0
	$MISSING_SHA1 missing
	EOF
'

test_expect_success 'lookup in duplicated pack (binary search)' '
	git cat-file --batch-check <input >actual &&
	test_cmp expect actual
'

test_expect_success 'lookup in duplicated pack (GIT_USE_LOOKUP)' '
	(
		GIT_USE_LOOKUP=1 &&
		export GIT_USE_LOOKUP &&
		git cat-file --batch-check <input >actual
	) &&
	test_cmp expect actual
'

test_done
