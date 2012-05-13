#!/bin/sh
#
# Copyright (c) 2009 Ilari Liusvaara
#

test_description='Test run command'

. ./test-lib.sh

test_expect_success 'start_command reports ENOENT' '
	test-run-command start-command-ENOENT ./does-not-exist
'

test_expect_success 'read_2_fds_into_strbuf basic behavior' '
	test-run-command out2strbuf >actual &&
	grep "^Hallo Stdout$" actual &&
	grep "^Hallo Stderr$" actual
'

test_done
