#!/bin/bash

set -uexo pipefail

# This script is invoked from HEAD of the source-git branch to be merged

GIT_SHA="$(git rev-parse HEAD)"

git clone --recursive https://git.twitter.biz/ro/sourcegit-dependencies
pushd sourcegit-dependencies
  # yes, this is putzing around with source-git inside sg-d inside source-git
  # SQ seems to require the jenkins workspace to be the repo being committed to,
  # but we are only able to build source-git as a submodule of sg-d
  pushd source-git
    git checkout "${GIT_SHA}"
  popd

  make test
popd

# if we exit successfully, SQ merges the branch
