#!/bin/sh
SUBDIRECTORY_OK=true
. "$(git --exec-path)/git-sh-setup"
. git-atomic-lib
atomic "$@"
