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

dh_graph_test() {
  local a="$1" b="$2" c="$3"

  {
    printf "$a" > file
    git add file
    git commit -m"Add a file"

    printf "$b" > file
    git commit -am"Update a file"

    git checkout -b branch
    printf "$c" > file
    git commit -am"Update a file on branch"

    git checkout master
    printf "$a" > file
    git commit -am"Update a file again"

    git checkout branch
    printf "$b" > file
    git commit -am"Update a file similar to master"

    git merge master
    git checkout master
    git merge branch --no-ff
  } >/dev/null 2>&1

  git log -p --graph --no-merges > graph.raw

  # git log --graph orders the commits different than git log so we hack it by
  # using sed to remove the graph part. We know from other tests, that CMD
  # works without the graph, so there should be no diff when running it with
  # and without.
  < graph.raw sed -e 's"^\(*\|| \||/\)\+""' -e 's"^  ""' | $CMD > graph.exp
  < graph.raw $CMD | sed -e 's"^\(*\|| \||/\)\+""' -e 's"^  ""' > graph.act

  # ignore whitespace since we're using a hacky sed command to remove the graph
  # parts.
  diff -b graph.exp graph.act
}
