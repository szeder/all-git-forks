#!/bin/sh

test_description='see if test-gitmw-lib.sh is correct'

. ./test-gitmw-lib.sh
. ./test-lib.sh
wiki_editpage bar "hello world" false
test_expect_success 'correct behavior' '
	wiki_getpage foo . &&
	rm foo &&
	wiki_delete_page bar &&
	wiki_page_exist foo &&
'
test_done



