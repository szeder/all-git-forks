#!/bin/sh
#
# Copyright (c) 2013 Mathieu Lienard--Mayor et Jorge Garcia
#

test_description='Test status.short and status.branch'

. ./test-lib.sh

test_expect_success '"status.short=true" same as "-s"' '
    >short_status &&
    >status &&
    git -c status.short=true status >short_status &&
    git status -s >status &&
    test_cmp status short_status
'

test_expect_success '"status.short=true" different from "--no-short"' '
    >short_status &&
    >status &&
    git -c status.short=true status >short_status &&
    git status --no-short >status &&
    test_must_fail test_cmp status short_status
'

test_expect_success '"status.short=true" weaker than "--no-short"' '
    >no-short_status &&
    >status &&
    git -c status.short=true status --no-short >no-short_status &&
    git status --no-short >status &&
    test_cmp status no-short_status
'

test_expect_success '"status.short=false" same as "--no-short"' '
    >shortfalse_status &&
    >status &&
    git -c status.short=false status >shortfalse_status &&
    git status --no-short >status &&
    test_cmp status shortfalse_status
'

test_expect_success '"status.short=false" different from "-s"' '
    >shortfalse_status &&
    >status &&
    git -c status.short=false status >shortfalse_status &&
    git status -s >status &&
    test_must_fail test_cmp status shortfalse_status
'

test_expect_success '"status.short=false" weaker than "-s"' '
    >shortfalse_status &&
    >status &&
    git -c status.short=false status -s >shortfalse_status &&
    git status -s >status &&
    test_cmp status shortfalse_status
'

test_done
