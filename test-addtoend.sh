#!/bin/sh

set -e

TEST_DIR=`mktemp -d`

t_git() {
    git --work-tree="$TEST_DIR" --git-dir="$TEST_DIR/.git" "$@"
}

t_git init
cat >"$TEST_DIR/file" <<EOF
line1
line2
line3
EOF
t_git add file
t_git commit -m 'file added'
cat >>"$TEST_DIR/file" <<EOF
line3
line4
EOF
cat >"$TEST_DIR/file2" <<EOF
line1
line2
line3
line4
line5
EOF
cat >"$TEST_DIR/file3" <<EOF
line1
line2
line3
line4
line5
EOF
t_git add file file2 file3
t_git commit -m 'lines added, file2 added'
cat >"$TEST_DIR/file2" <<EOF
line1
line2
line3 line4
line5
EOF
t_git commit -m '-newline' file2
cat >"$TEST_DIR/file3" <<EOF
line1
line2
 line3
line4
line5
EOF
t_git commit -m 'change indent' file3

echo "Run GIT_DIR=$TEST_DIR/.git ./gitk"

