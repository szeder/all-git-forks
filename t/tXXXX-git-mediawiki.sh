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

. ./test-lib.sh
. ./test-gitmw-lib.sh

test_expect_success 'git clone works on mediawiki::...' '

        for each name in file do
                ./wiki_pagecontent "name"".mw" "name"
	done
'

test_expect_success 'git clone works with page added' '
	#create_wiki
	wiki_editpage foo "hello_world" true &&
	wiki_editpage bar "hi everybody !" true &&
	git clone mediawiki::http://localhost/mediawiki

'

test_done
