#!/bin/sh

test_description='see if test-gitmw-lib.sh is correct'

#. ./install_wiki.sh
. ./test-gitmw-lib.sh
. ./test-lib.sh

#test_expect_success 'install a wiki' '
#	./install_wiki "install"
#'


test_expect_success 'correct behavior' '
	wiki_editpage bar '\''Bar Bar !'\'' false &&
	wiki_editpage foo '\''foooooooooo'\'' false &&
	wiki_getpage foo . &&
	test_path_is_file foo.mw &&
	rm foo.mw &&
	wiki_delete_page bar &&
	wiki_page_exist foo
'

test_expect_success 'test of file manipulation' '
	wiki_editpage barbar "hello world" false &&
	touch barbar.test &&
	echo "hello world" > barbar.test &&
	test_path_is_file barbar.test &&
	wiki_page_content barbar.test barbar
'

test_expect_success 'Get ALL' '
	wiki_getallpage dir
'
test_done
