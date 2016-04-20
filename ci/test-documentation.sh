#!/bin/sh
#
# Perform a sanity checks on documentation and build it.
#

set -e

LINKS=$(grep --recursive --only-matching --no-filename --perl-regexp \
    '(?<=linkgit:).*?(?=\[\d+\])' Documentation/* \
    | sort -u \
)

for LINK in $LINKS; do
    echo "Checking linkgit:$LINK..."
    test -s Documentation/$LINK.txt
done

make check-builtins
make check-docs
make doc

test -s Documentation/git.html
test -s Documentation/git.xml
test -s Documentation/git.1
