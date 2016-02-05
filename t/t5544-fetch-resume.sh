#!/bin/sh

test_description='what'

. ./test-lib.sh

test_expect_success 'what' '
	test_commit one &&
	git clone --no-local .git abc &&
	(
	cd abc &&
	mv `ls .git/objects/pack/*.pack` pack &&
	git unpack-objects < pack &&
	rm pack &&
	git fsck
	) &&
	test_commit two &&
	test_commit three &&
	(
	cd abc &&
	git fetch --resume-pack=foo origin HEAD &&
	git log --format=%s origin/master >actual &&
	echo one >expected &&
	test_cmp expected actual &&
	rm .git/FETCH_HEAD &&
	mv `ls .git/objects/pack/*.pack` pack &&
	head -c 123 pack >tmp &&
	git fetch --resume-pack=tmp origin &&
	test_path_is_missing tmp &&
	cmp pack .git/objects/pack/*.pack &&
	git fsck &&
	git log --format=%s origin/master >actual &&
	cat >expected <<EOF &&
three
two
one
EOF
	test_cmp expected actual
	)
'

test_done
