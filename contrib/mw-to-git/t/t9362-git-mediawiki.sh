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

test_description='Test git-mediawiki with special characters in filenames'

. ./test-gitmw-lib.sh
. $TEST_DIRECTORY/test-lib.sh

TRASH_DIR=$CURR_DIR/trash\\\ directory.t9362-git-mediawiki

if ! test_have_prereq PERL
then
	skip_all='skipping gateway git-mw tests, '\
		'perl not available'
	test_done
fi

if [ ! -f $GIT_BUILD_DIR/git-remote-mediawiki ];
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

test_expect_failure 'capital at the begining of file names' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	cd mw_dir &&
	echo "my new file foo" > foo.mw &&
	echo "my new file Foo... Finger crossed" > Foo.mw &&
	git add . &&
	git commit -am "file foo.mw" &&
	git pull &&
	git push &&
	cd .. &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page
'

test_expect_failure 'special character at the begining of file name from mw to git' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_editpage {char_1 "expect to be renamed {char_1" false &&
	wiki_editpage [char_2 "expect to be renamed [char_2" false &&
	cd mw_dir &&
	git pull
	cd .. &&
	test -f mw_dir/{char_1 &&
	test -f mw_dir/[char_2
'

test_expect_success 'test of correct formating for file name from mw to git' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_editpage char{_1 "expect to be renamed char{_1" false &&
	wiki_editpage char[_2 "expect to be renamed char{_2" false &&
	cd mw_dir &&
	git pull
	cd .. &&
	test -f mw_dir/Char\{_1.mw &&
	test -f mw_dir/Char\[_2.mw &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page
'

test_expect_failure 'test of correct formating for file name begining with special character' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	cd mw_dir &&
	echo "my new file {char_1" > \{char_1.mw &&
	echo "my new file [char_2" > \[char_2.mw &&
	git add . &&
	git commit -am "commiting some exotic file name..." &&
	git push &&
	git pull &&
	cd .. &&
	wiki_getallpage ref_page &&
	test -f ref_page/{char_1.mw &&
	test -f ref_page/[char_2.mw &&
	test_diff_directories mw_dir ref_page
'


test_expect_success 'test of correct formating for file name from git to mw' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	cd mw_dir &&
	echo "my new file char{_1" > Char\{_1.mw &&
	echo "my new file char[_2" > Char\[_2.mw &&
	git add . &&
	git commit -m "commiting some exotic file name..." &&
	git push &&
	cd .. &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page
'
# 1
# Clone a wiki and check that there are no problems with accents in pages
test_expect_success 'Git clone works for a wiki with accents in page names' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
	wiki_editpage féé "This page must be délétéd before clone" false &&
	wiki_editpage kèè "This page must be deleted before clone" false &&
	wiki_editpage hàà "This page must be deleted before clone" false &&
	wiki_editpage kîî "This page must be deleted before clone" false &&
	wiki_editpage foo "This page must be deleted before clone" false &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page
'     

# 2
# Create a new page with accents in its name on a cloned wiki
# Check that git pull works with accents
test_expect_success 'Git pull works with a wiki with accents' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage kîî "This page must be deleted before clone" false &&
	wiki_editpage foo "This page must be deleted before clone" false &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_editpage éàîôû "This page must be deleted before clone" false &&
	cd mw_dir &&
	git pull &&
	cd .. && 
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page &&
	rm -rf mw_dir &&
	rm -rf ref_page
'

# 3
# Clone only one page and check that there are no problems
test_expect_success 'Cloning a chosen page works with accents' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage kîî "This page must be delete before the clone" false &&
	git clone -c remote.origin.pages=kîî mediawiki::http://localhost/wiki mw_dir &&
	wiki_check_content mw_dir/Kîî.mw Kîî &&
	test -e mw_dir/Kîî.mw &&
	rm -rf mw_dir
'

# 4
# Use the shallow option for the clone
# Check that there are no problems with accents
test_expect_success 'The shallow option works with accents' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage néoà "1st revision, should not be cloned" false &&
	wiki_editpage néoà "2nd revision, should be cloned" false &&
	git -c remote.origin.shallow=true clone mediawiki::http://localhost/wiki/ mw_dir &&
        test_contains_N_files mw_dir 2 &&
	test -e mw_dir/Néoà.mw &&
	test -e mw_dir/Main_Page.mw &&
	cd mw_dir &&
	test `git log --oneline Néoà.mw | wc -l` -eq 1 &&
	test `git log --oneline Main_Page.mw | wc -l ` -eq 1 &&
	cd .. &&
	wiki_check_content mw_dir/Néoà.mw Néoà &&
	wiki_check_content mw_dir/Main_Page.mw Main_Page &&
	rm -rf mw_dir
'

# 5
# Check that having an accent on the first letter does not cause any problem
test_expect_success 'Cloning works when page name first letter has an accent' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage îî "This page must be deleted before clone" false &&
	git clone -c remote.origin.pages=îî mediawiki::http://localhost/wiki mw_dir &&
	test -e mw_dir/Îî.mw &&
	wiki_check_content mw_dir/Îî.mw Îî &&
	rm -rf mw_dir
'

# 6
# Git push works with accents
test_expect_success 'Git push works with a wiki with accents' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage féé "This page must be délétéd before clone" false &&
	wiki_editpage foo "This page must be deleted before clone" false &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	cd mw_dir &&
	echo "A wild Pîkächû appears on the wiki" > Pîkächû.mw &&
	git add Pîkächû.mw &&
	git commit -m "A new page appears" &&
	git push &&
	cd .. &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page &&
	rm -rf mw_dir &&
	rm -rf ref_page
'     
     
# 7
# Accents and spaces works together
test_expect_success 'Git push works with a wiki with accents' '
	wiki_reset &&
	cd '"$TRASH_DIR"' &&
	rm -rf mw_dir &&
	rm -rf ref_page

	wiki_editpage "é à î" "This page must be délétéd before clone" false &&
	git clone mediawiki::http://localhost/wiki mw_dir &&
	wiki_getallpage ref_page &&
	test_diff_directories mw_dir ref_page &&
	rm -rf mw_dir &&
	rm -rf ref_page
'

 test_done
