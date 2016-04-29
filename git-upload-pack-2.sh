#!/bin/sh
#
# Copyright (c) 2016 Twitter, Inc


. git-sh-setup

exec git upload-pack --transport-version=2 "$@"
