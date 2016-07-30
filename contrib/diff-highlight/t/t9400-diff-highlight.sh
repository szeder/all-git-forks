#!/bin/sh
#
# Copyright (C) 2016

test_description='Test diff-highlight'

. ./test-diff-highlight.sh
. $TEST_DIRECTORY/test-lib.sh

# PERL is required, but assumed to be present, although not necessarily modern
# some tests require 5.8

test_expect_success 'diff-highlight highlightes the beginning of a line' '
  dh_test \
    "aaa\nbbb\nccc\n" \
    "aaa\n0bb\nccc\n" \
"
 aaa
-${CW}b${CR}bb
+${CW}0${CR}bb
 ccc
"
'

test_expect_success 'diff-highlight highlightes the end of a line' '
  dh_test \
    "aaa\nbbb\nccc\n" \
    "aaa\nbb0\nccc\n" \
"
 aaa
-bb${CW}b${CR}
+bb${CW}0${CR}
 ccc
"
'

test_expect_success 'diff-highlight highlightes the middle of a line' '
  dh_test \
    "aaa\nbbb\nccc\n" \
    "aaa\nb0b\nccc\n" \
"
 aaa
-b${CW}b${CR}b
+b${CW}0${CR}b
 ccc
"
'

test_expect_success 'diff-highlight does not highlight whole line' '
  dh_test \
    "aaa\nbbb\nccc\n" \
    "aaa\n000\nccc\n"
'

test_expect_success 'diff-highlight does not highlight mismatched hunk size' '
  dh_test \
    "aaa\nbbb\n" \
    "aaa\nb0b\nccc\n"
'

# TODO add multi-byte test

test_expect_success 'diff-highlight highlightes the beginning of a line' '
  dh_graph_test \
    "aaa\nbbb\nccc\n" \
    "aaa\n0bb\nccc\n" \
    "aaa\nb0b\nccc\n" \
"
 aaa
-${CW}b${CR}bb
+${CW}0${CR}bb
 ccc
"
'

test_done
