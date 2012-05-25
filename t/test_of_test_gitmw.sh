#!/bin/sh

test_description='see if test-gitmw-lib.sh is correct'

. ./test-gitmw-lib.sh
. ./test-lib.sh

if test -z "$GIT_TEST_HTTPD"
then
	skip_all="Network testing disabled (define GIT_TEST_HTTPD to enable)"
	test_done
fi


test_expect_success 'correct behavior' '
	wiki_editpage bar "hello world" false &&
	wiki_editpage foo "AHAHAHAH" false &&
	wiki_getpage foo . &&
	rm foo &&
	wiki_delete_page bar &&
	wiki_page_exist foo
'

test_expect_success 'test of file manipulation' '
	wiki_editpage bar "hello world" false &&
	touch bar.test &&
	echo "hello world" > bar.test &&
	git_exist . bar.test &&
	wiki_page_content bar.test bar
'
test_done



