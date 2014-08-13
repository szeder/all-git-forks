#!/bin/sh

test_description='test git-specific Zsh prompt functions'

. ./lib-zsh.sh
. "$TEST_DIRECTORY"/lib-prompt-tests.sh

run_prompt_tests

test_done
