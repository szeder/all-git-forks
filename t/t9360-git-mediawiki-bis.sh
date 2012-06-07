#!/bin/sh
#
# Copyright (C) 2012
#     Charles Roussel <charles.roussel@ensimag.imag.fr>
#     Simon Cathebras <simon.cathebras@ensimag.imag.fr>
#     Julien Khayat <julien.khayat@ensimag.imag.fr>
#     Guillaume Sasdy <guillaume.sasdy@ensimag.imag.fr>
#     Simon Perrat <simon.perrat@ensimag.imag.fr>
#
# License: GPL v2 or later

# tests for git-remote-mediawiki

test_description='Test the Git Mediawiki remote helper: git clone'

. ./test-gitmw-lib.sh
. ./test-lib.sh

if ! test_have_prereq PERL
then
	skip_all='skipping gateway git-mw tests, '\
		'perl not available'
	test_done
fi

if [ ! -f /$GIT_BUILD_DIR/git-remote-mediawiki ];
then
	skip_all='skipping gateway git-mw tests,' \
		' no remote mediawiki for git found'
	test_done
fi

if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ;
then
	skip_all='skipping gateway git-mw tests, no mediawiki found'
	test_done
fi

# 6ème
# clone only one page of the wiki
# check that it is the only page cloned
# check that the page and the clone a identical
test_expect_success 'git clone works one specific page cloned ' '
        wiki_reset &&
        wiki_editpage foo "I will not be cloned" false &&
        wiki_editpage bar "Do not clone me" false &&
        wiki_editpage namnam "I will be cloned :)" false &&
        wiki_editpage nyancat "nyan nyan nyan you cant clone me" false &&
        git clone -c remote.origin.pages=namnam mediawiki::http://localhost/wiki mw_dir &&
        test `ls mw_dir | wc -l` -eq 1 &&
        test -e mw_dir/Namnam.mw &&
        test ! -e mw_dir/Main_Page.mw &&
        wiki_check_content mw_dir/Namnam.mw Namnam &&
        rm -rf mw_dir
'   
test_done
