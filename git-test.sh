#!/bin/sh
#
# (c) Copyright Jon Seymour 2010
#
SUBDIRECTORY_OK=true
. "$(git --exec-path)/git-sh-setup"
. git-test-lib

require_condition_libs

assert "$@"
