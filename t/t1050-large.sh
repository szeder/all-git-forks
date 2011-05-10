#!/bin/sh
# Copyright (c) 2011, Google Inc.

test_description='adding and checking out large blobs'

. ./test-lib.sh

test_expect_success setup '
	git config core.bigfilethreshold 200k &&
	echo X | dd of=large bs=1k seek=2000
'

test_expect_success 'add a large file' '
	git add large &&
	git cat-file blob :large >actual &&
	# make sure we got a packfile and no loose objects
	test -f .git/objects/pack/pack-*.pack &&
	test ! -f .git/objects/??/?????????????????????????????????????? &&
	cmp -s large actual  # This should be "cmp", not "test_cmp"
'

test_done
