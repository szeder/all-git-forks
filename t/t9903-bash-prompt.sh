#!/bin/sh
#
# Copyright (c) 2012 SZEDER Gábor
#

test_description='test git-specific bash prompt functions'

. ./lib-bash.sh
. "$TEST_DIRECTORY"/lib-prompt-tests.sh

run_prompt_tests

test_done
