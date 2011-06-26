#!/bin/sh

test_description='Test cherry-pick continuation features

  + picked: rewrites foo to c
  + unrelatedpick: rewrites unrelated to reallyunrelated
  + base: rewrites foo to b
  + initial: writes foo as a, unrelated as unrelated

'

. ./test-lib.sh

pristine_detach () {
	git checkout -f "$1^0" &&
	git read-tree -u --reset HEAD &&
	git clean -d -f -f -q -x
}

test_expect_success setup '
	echo unrelated >unrelated &&
	git add unrelated &&
	test_commit initial foo a &&
	test_commit base foo b &&
	test_commit unrelatedpick unrelated reallyunrelated &&
	test_commit picked foo c &&
	git config advice.detachedhead false

'

test_expect_success 'cherry-pick cleans up sequencer directory upon success' '
	pristine_detach initial &&
	git cherry-pick initial..picked &&
	test_path_is_missing .git/sequencer
'

test_expect_success '--skip-all complains when no cherry-pick is in progress' '
	pristine_detach initial &&
	test_must_fail git cherry-pick --skip-all >actual 2>&1 &&
	test_i18ngrep "error" actual
'

test_expect_success '--skip-all cleans up sequencer directory' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	git cherry-pick --skip-all &&
	test_must_fail test -d .git/sequencer
'

test_expect_success '--continue complains when no cherry-pick is in progress' '
	pristine_detach initial &&
	test_must_fail git cherry-pick --continue >actual 2>&1 &&
	test_i18ngrep "error" actual
'

test_expect_success '--continue complains when there are unresolved conflicts' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	test_must_fail git cherry-pick --continue &&
	git cherry-pick --skip-all
'

test_expect_success '--continue continues after conflicts are resolved' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	echo "resolved" >foo &&
	git add foo &&
	git commit &&
	git cherry-pick --continue &&
	test_must_fail test -d .git/sequencer &&
	git rev-list --count HEAD >actual &&
	echo 3 >expect &&
	test_cmp expect actual
'

test_done
