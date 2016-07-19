#!/bin/sh

test_description='blob conversion via gitattributes'

. ./test-lib.sh

cat <<EOF >rot13.sh
#!$SHELL_PATH
tr \
  'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ' \
  'nopqrstuvwxyzabcdefghijklmNOPQRSTUVWXYZABCDEFGHIJKLM'
EOF
chmod +x rot13.sh

test_expect_success setup PERL 'test batched git-lfs smudge/clean filter' '
    # invocation goes here
'
