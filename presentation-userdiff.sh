#!/bin/sh

test_description='css diff colors'
cd t

. ./test-lib.sh
. "$TEST_DIRECTORY"/diff-lib.sh

word_diff () {
	git diff --no-index "$@" pre post
}

    cp "$TEST_DIRECTORY/t4034/css/pre" \
        "$TEST_DIRECTORY/t4034/css/post" \
        "$TEST_DIRECTORY/t4034/css/expect" . &&
    word_diff --color-words;
    echo
    echo "-------------------------------------------" &&
    echo "-------------------------------------------" &&
    echo
    echo "* diff=css" >.gitattributes &&
    word_diff --color-words


test_done
