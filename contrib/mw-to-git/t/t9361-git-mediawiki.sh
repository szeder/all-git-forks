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

test_description='Test the Git Mediawiki remote helper: git push and git pull simple test cases'

. ./test-gitmw-lib.sh
. $TEST_DIRECTORY/test-lib.sh

TRASH_DIR=$CURR_DIR/trash\\\ directory.t9361-git-mediawiki

if ! test_have_prereq PERL
then
	skip_all='skipping gateway git-mw tests, perl not available'
	test_done
fi

if [ ! -f $GIT_BUILD_DIR/git-remote-mediawiki ];
then
	skip_all='skipping gateway git-mw tests, no remote mediawiki for git found'
	test_done
fi

if [ ! -d "$WIKI_DIR_INST/$WIKI_DIR_NAME" ] ;
then
	skip_all='skipping gateway git-mw tests, no mediawiki found'
	test_done
fi


# Create a new file foo.mw in a freshly cloned wiki. Commit and push
# this file and then check that all files of the repository are similar to
# the wiki pages.
test_expect_success 'git push works after adding a file .mw' "
	wiki_reset &&
	cd $TRASH_DIR &&
	git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&
        wiki_getallpage ref_page &&
        cd mw_dir &&
        test ! -f Foo.mw && # Foo.mw should not exist after wiki_reset &&
        touch Foo.mw &&
        echo \"hello world\" >> Foo.mw &&
        git add Foo.mw &&
        git commit -m \"Foo\" &&
        git push &&
        cd .. &&

        rm -rf ref_page &&
        wiki_getallpage ref_page &&
        test_diff_directories mw_dir ref_page &&
        rm -rf ref_page &&
        rm -rf mw_dir
"

# Create a new page Foo in an empty wiki. Clone the wiki and edit
# the file foo.mw corresponding to the page Foo. Commit and push
# then check that all files of the repository are similar to the wiki
# pages.
test_expect_success 'git push works after editing a file .mw' "
        # clone an empty wiki and add a Foo page
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
        wiki_editpage \"Foo\" \"page just created before the git clone\" false &&
        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&

        cd mw_dir &&
        echo \"new line added in the file Foo.mw\" >> Foo.mw &&
        git commit -am \"edit file Foo.mw\" &&
        git push &&
        cd .. &&

        rm -rf ref_page &&
        wiki_getallpage ref_page &&
        test_diff_directories mw_dir ref_page &&
        rm -rf ref_page &&
        rm -rf mw_dir
"

# Clone a wiki with a page Foo. Remove the corresponding file from the
# repository. Check that the wiki page has been actually removed.
test_expect_failure 'git push works after deleting a file' "
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&
        wiki_editpage Foo \"wiki page added before git clone\" false &&
        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&

        cd mw_dir &&
        git rm Foo.mw &&
        git commit -am \"git rm delete the wiki page Foo.mw\" &&
        git push &&
        cd .. &&

        rm -rf mw_dir &&
        test ! wiki_page_exist Foo
"

# Clone an empty wiki. Add a page on the wiki and pull. Check 
# all files of the repository are similar to the wiki pages.
test_expect_success 'git pull works after adding a new wiki page' "
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&

        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&
        wiki_editpage Foo \"page just created after the git clone\" false &&

        cd mw_dir &&
        git pull &&
        cd .. &&

        rm -rf ref_page &&
        wiki_getallpage ref_page &&
        test_diff_directories mw_dir ref_page &&
        rm -rf ref_page &&
        rm -rf mw_dir
"

# Clone a wiki with a page Foo. Edit the page foo on the wiki and pull.
# Check that all files of the repository are similar to the wiki pages.
test_expect_success 'git pull works after editing a wiki page' "
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&

        wiki_editpage Foo \"page just created before the git clone\" false &&
        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&
        wiki_editpage Foo \"new line added on the wiki\" true &&

        cd mw_dir &&
        git pull &&
        cd .. &&

        rm -rf ref_page &&
        wiki_getallpage ref_page &&
        test_diff_directories mw_dir ref_page &&
        rm -rf ref_page &&
        rm -rf mw_dir
"

# Clone a wiki with a page Foo. Edit both the page Foo on the wiki and the file
# Foo.mw on git, then check that merge conflict is properly coped with.
test_expect_success 'git pull works on conflict handled by auto-merge' "
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&

        wiki_editpage Foo \"1 init
3
5
\" false &&
        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&

        wiki_editpage Foo \"1 init
2 content added on wiki after clone
3
5
\" false &&

        cd mw_dir &&
        echo \"1 init
3
4 content added on git after clone
5
\" > Foo.mw &&
        git commit -am \"conflicting change on foo\" &&
        git pull &&
        git push &&
        cd .. &&

        rm -rf mw_dir
"

# The two following tests generate a merge conflict, and then handle it
# manually
test_expect_success 'Merge conflict expected' "
        wiki_reset &&
	cd $TRASH_DIR &&
	rm -rf mw_dir &&
	rm -rf ref_page &&

        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&
        wiki_editpage Foo \"1 conflict
3 wiki
4\" false &&

        cd mw_dir &&
        echo \"1 conflict
2 git
4\" > Foo.mw &&
        git add Foo.mw &&
        git commit -m \"conflict\" &&
        test_must_fail git pull
"

test_expect_success 'Conflict solved manually' "
        cat Foo.mw | sed '/[<=>][^$]*/ d' > Foo.mw~ &&
        cat Foo.mw~ > Foo.mw &&
        git commit -am \"merge conflict\" &&
        git push &&

        cd .. &&
        rm -rf mw_dir
"

# Clone a wiki with a page Foo. Remove the wiki page and pull.
# Check that the corresponding file in the repository has been actually removed.
test_expect_failure 'git pull works after deleting a wiki page' "
        wiki_reset &&
	cd $TRASH_DIR &&
        wiki_editpage Foo \"wiki page added before git clone\" false &&
        git clone mediawiki::http://$SERVER_ADDR/$WIKI_DIR_NAME mw_dir &&

        wiki_delete_page Foo &&
        cd mw_dir &&
        git pull &&
        test ! -f Foo.mw &&
        cd .. &&

        rm -rf mw_dir
"

test_done

