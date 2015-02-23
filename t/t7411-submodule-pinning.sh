#!/bin/sh
#
# Copyright (c) 2015 Anders Ronnbrant
#

test_description="Branch name '@' disables submodule update --remote calls"

. ./test-lib.sh

get_sha() {
  cd $1 && git rev-list --max-count=1 HEAD
}

equal_sha() {
  test "$(get_sha $1)" = "$(get_sha $2)"
}

not_equal_sha() {
  test "$(get_sha $1)" != "$(get_sha $2)"
}

test_expect_success 'setup submodule tree structure' '
for i in 1 2 3; do echo file$i > file$i; git add file$i; git commit -m file$i; done &&
test_tick &&
git clone . super &&
git clone . follow &&
git clone . pinned &&
(cd super && git submodule add -b master ../follow follow) &&
(cd super && git submodule add           ../pinned pinned)
'

test_expect_success 'verify submodules have the same SHA' '
equal_sha super/follow super/pinned
'


test_expect_success 'switch submodule pinned to HEAD~1 and commit super' '
(cd super/pinned && git checkout master && git reset --hard HEAD~1) &&
(cd super && git add pinned && git commit -m "Submodule pinned @ HEAD~1") &&
(cd super && git submodule status)
'


test_expect_success 'verify submodules not have the same SHA anymore' '
not_equal_sha super/follow super/pinned
'


test_expect_success 'set branch name to "@" for submodule pinned' '
(cd super && git config --replace-all submodule.pinned.branch "@") &&
test "$(cd super && git config --get submodule.pinned.branch)" = "@"
'


test_expect_success 'run submodule update --remote and expect no change' '
(cd super && git submodule update --remote) &&
not_equal_sha super/follow super/pinned
'


test_expect_success 'remove branch name "@" for submodule pinned (unpin)' '
(cd super && git config --unset-all submodule.pinned.branch) &&
(cd super && git config --list)
'


test_expect_success 'run submodule update --remote and expect same SHA1 again' '
(cd super && git submodule update --remote) &&
equal_sha super/follow super/pinned
'


test_done
