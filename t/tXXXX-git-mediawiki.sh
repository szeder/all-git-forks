#!/bin/sh
#
# Copyright (C) 2012
#     Charles Roussel <charles.roussel@ensimag.imag.fr>
#     Simon Cathebras <simon.cathebras@ensimag.imag.fr>
#     Julien Khayat <julien.khayat@ensimag.imag.fr>
#     Guillaume Sasdy <guillaume.sasdy@ensimag.imag.fr>
#     Simon Perrat <simon.perrat@ensimag.imag.fr>
# License:

# tests for git-mediawiki

test_description='Test the Git Mediawiki remote helper'

. ./test-gitmw-lib.sh
. ./test-lib.sh

if ! test_have_prereq PERL
then
	skip_all='skipping gateway git-mw tests, perl not available'
	test_done
fi

if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ;
then
	skip_all='skipping gateway git-mw tests, no mediawiki found'
	test_done
fi


test_expect_success 'git clone works with page added' '
	cmd_reset &&
	wiki_editpage foo "hello_world" false &&
	wiki_editpage bar "hi everybody !" false &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_getallpage ref_page &&
	git_content mw_dir ref_page &&
	wiki_delete_page foo &&
	wiki_delete_page bar
'

test_done
