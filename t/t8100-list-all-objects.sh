#!/bin/sh

test_description='git list-all-objects'
. ./test-lib.sh

test_expect_success 'setup' '
	echo hello, world >file &&
	git add file &&
	git commit -m "initial"
'

test_basic_repo_objects () {
	git cat-file --batch-check="%(objectname)" <<-EOF >expected.unsorted &&
		HEAD
		HEAD:file
		HEAD^{tree}
	EOF
	git list-all-objects >all-objects.unsorted &&
	sort expected.unsorted >expected &&
	sort all-objects.unsorted >all-objects &&
	test_cmp all-objects expected
}

test_expect_success 'list all objects' '
	test_basic_repo_objects
'
test_expect_success 'list all objects after pack' '
	git repack -Ad &&
	test_basic_repo_objects
'

test_expect_success 'verbose output' '
	git cat-file --batch-check="%(objectname) %(objecttype) %(objectsize)" \
			<<-EOF >expected.unsorted &&
		HEAD
		HEAD:file
		HEAD^{tree}
	EOF
	git list-all-objects -v >all-objects.unsorted &&
	sort expected.unsorted >expected &&
	sort all-objects.unsorted >all-objects &&
	test_cmp all-objects expected
'

test_done
