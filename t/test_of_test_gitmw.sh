#!/bin/sh

test_description='see if test-gitmw-lib.sh is correct'

. ./test-gitmw-lib.sh
. ./test-lib.sh



test_expect_success 'correct behavior' '
	wiki_editpage bar "hello world" false &&
	wiki_editpage foo "AHAHAHAH" false &&
	wiki_getpage foo . &&
	rm foo &&
	wiki_delete_page bar &&
	wiki_page_exist foo
'
test_done



