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

test_expect_success '--reset complains when no cherry-pick is in progress' '
	pristine_detach initial &&
	test_must_fail git cherry-pick --reset >actual 2>&1 &&
	test_i18ngrep "error" actual
'

test_expect_success '--reset cleans up sequencer directory' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	git cherry-pick --reset &&
	test_path_is_missing .git/sequencer
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
	git cherry-pick --reset
'

test_expect_success '--continue continues after conflicts are resolved' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	echo "resolved" >foo &&
	git add foo &&
	git commit &&
	git cherry-pick --continue &&
	test_path_is_missing .git/sequencer &&
	{
		git rev-list HEAD |
		git diff-tree --root --stdin |
		sed "s/[0-9a-f]\{40\}/OBJID/g"
	} >actual &&
	cat >expect <<-\EOF &&
	OBJID
	:100644 100644 OBJID OBJID M	foo
	OBJID
	:100644 100644 OBJID OBJID M	unrelated
	OBJID
	:000000 100644 OBJID OBJID A	foo
	:000000 100644 OBJID OBJID A	unrelated
	EOF
	test_cmp expect actual
'

test_expect_success 'malformed instruction sheet 1' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	echo "resolved" >foo &&
	git add foo &&
	git commit &&
	sed "s/pick /pick/" .git/sequencer/todo >new_sheet
	cp new_sheet .git/sequencer/todo
	test_must_fail git cherry-pick --continue >actual 2>&1 &&
	git cherry-pick --reset &&
	test_i18ngrep "fatal" actual
'

test_expect_success 'malformed instruction sheet 2' '
	pristine_detach initial &&
	head=$(git rev-parse HEAD) &&
	test_must_fail git cherry-pick base..picked &&
	echo "resolved" >foo &&
	git add foo &&
	git commit &&
	sed "s/pick/revert/" .git/sequencer/todo >new_sheet
	cp new_sheet .git/sequencer/todo
	test_must_fail git cherry-pick --continue >actual 2>&1 &&
	git cherry-pick --reset &&
	test_i18ngrep "fatal" actual
'

test_done
