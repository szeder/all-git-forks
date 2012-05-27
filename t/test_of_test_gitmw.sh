#!/bin/sh

test_description='see if test-gitmw-lib.sh is correct'

#. ./install_wiki.sh
. ./test-gitmw-lib.sh
. ./test-lib.sh

#test_expect_success 'install a wiki' '
#	./install_wiki "install"
#'


test_expect_success 'correct behavior' '
	wiki_editpage bar "hello world" false &&
	wiki_editpage foo "AHAHAHAH" false &&
	wiki_getpage foo . &&
	test_path_is_file foo.mw &&
	rm foo.mw &&
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

test_expect_success 'Get ALL' '
	wiki_getallpage Foo
'
test_done
