#!/bin/sh
#
# Perform a quick sanity check on documentation generated with 'make doc'.
#

set -e

test_file_count () {
    SUFFIX=$1
    EXPECTED_COUNT=$2
    ACTUAL_COUNT=$(find Documentation -type f -name "*.$SUFFIX" | wc -l)
    echo "$ACTUAL_COUNT *.$SUFFIX files found. $EXPECTED_COUNT expected."
    test $ACTUAL_COUNT -eq $EXPECTED_COUNT
}

test -s Documentation/git.html
test -s Documentation/git.xml
test -s Documentation/git.1

# The follow numbers need to be adjusted when new documentation is added.
test_file_count html 233
test_file_count xml 171
test_file_count 1 152
