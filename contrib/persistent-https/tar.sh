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

# Builds the binaries for release and produces them in a tarball.

set -e

OUT=$(go env GOOS)_$(go env GOARCH).tar.gz

rm -rf bin git-remote-persistent-https ${OUT}

./release.sh

echo
tput setaf 2 # green
echo "Creating ${OUT}:"
tput sgr0

mkdir bin
cp COPYING README bin
chmod 444 bin/*
mv git-remote-persistent-http* bin
mv bin git-remote-persistent-https

tar -czvf ${OUT} git-remote-persistent-https

rm -rf git-remote-persistent-https
