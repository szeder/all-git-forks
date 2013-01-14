#!/bin/sh

GIT_CVSPS_VERSION=2

exec git cvsimport-$GIT_CVSPS_VERSION "$@"
