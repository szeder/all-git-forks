#!/bin/sh
#
# Copyright (C) 2016

CURR_DIR=$(pwd)
TEST_OUTPUT_DIRECTORY=$(pwd)
TEST_DIRECTORY="$CURR_DIR"/../../../t
cmd=diff-highlight
CMD="$CURR_DIR"/../$cmd

CW="\033[7m"
CR="\033[27m"

export TEST_OUTPUT_DIRECTORY TEST_DIRECTORY CW CR

dh_test() {
  dh_diff_test "$@"
  dh_commit_test "$@"
}

dh_diff_test() {
  local a="$1" b="$2"

  printf "$a" > file
  git add file

  printf "$b" > file

  git diff file > diff.raw

  if test "$#" = 3
  then
    # remove last newline
    head -n5 diff.raw | head -c -1 > diff.act
    printf "$3" >> diff.act
  else
    cat diff.raw > diff.act
  fi

  < diff.raw $CMD > diff.exp

  diff diff.exp diff.act
}

dh_commit_test() {
  local a="$1" b="$2"

  printf "$a" > file
  git add file
  git commit -m"Add a file" >/dev/null

  printf "$b" > file
  git commit -am"Update a file" >/dev/null

  git show > commit.raw

  if test "$#" = 3
  then
    # remove last newline
    head -n11 commit.raw | head -c -1 > commit.act
    printf "$3" >> commit.act
  else
    cat commit.raw > commit.act
  fi

  < commit.raw $CMD > commit.exp

  diff commit.exp commit.act
}
