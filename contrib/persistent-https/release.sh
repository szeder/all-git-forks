#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Builds the binaries for release. The script is mainly used to set the
#_BUILD_EMBED_LABEL to the current system time through a linker flag.

set -e

rm -f git-remote-persistent-https \
  git-remote-persistent-https--proxy \
  git-remote-persistent-http

BUILD_LABEL=`date +"%s"`
go build -o git-remote-persistent-https \
  -ldflags "-X main._BUILD_EMBED_LABEL $BUILD_LABEL"

chmod 555 git-remote-persistent-https
ln -s git-remote-persistent-https git-remote-persistent-https--proxy
ln -s git-remote-persistent-https git-remote-persistent-http

LIVE_LABEL=`./git-remote-persistent-https --print_label`

tput setaf 2 # green
echo "Successfully built with label: $LIVE_LABEL"
tput sgr0

echo
echo "Move the following binaries to a directory in your PATH:"
echo "  git-remote-persistent-https"
echo "  git-remote-persistent-https--proxy"
echo "  git-remote-persistent-http"
