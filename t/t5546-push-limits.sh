#!/bin/sh

test_description='check input limits for pushing'
. ./test-lib.sh

test_expect_success 'create remote repository' '
	git init --bare dest
'

# Let's run tests with different unpack limits: 1 and 10
# When the limit is 1, `git receive-pack` will call `git index-pack`.
# When the limit is 10, `git receive-pack` will call `git unpack-objects`.

while read unpacklimit filesize filename seed
do

	test_expect_success "create known-size ($filesize bytes) commit '$filename'" '
		test-genrandom "$seed" "$filesize" >"$filename" &&
		git add "$filename" &&
		test_commit "$filename"
	'

	test_expect_success "set unpacklimit to $unpacklimit" '
		git --git-dir=dest config receive.unpacklimit "$unpacklimit"
	'

	test_expect_success 'setting receive.maxInputSize to 512 rejects push' '
		git --git-dir=dest config receive.maxInputSize 512 &&
		test_must_fail git push dest HEAD
	'

	test_expect_success 'bumping limit to 4k allows push' '
		git --git-dir=dest config receive.maxInputSize 4k &&
		git push dest HEAD
	'

done <<\EOF
1 1024 one-k-file foo
10 1024 other-one-k-file bar
EOF

test_done
