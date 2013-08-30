#!/bin/sh

test_description='test for rev-list --except'

. ./test-lib.sh

test_expect_success 'setup' '

	echo one > content &&
	git add content &&
	git commit -m one &&
	git checkout -b test master &&
	echo two > content &&
	git commit -a -m two &&
	git checkout -b merge master &&
	git merge test
'

test_expect_success 'rev-list --except' '

	git rev-list --topo-order --branches --except merge > actual &&
	git rev-list --topo-order test > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list --except with extra' '

	echo three > content &&
	git commit -a -m three &&
	git rev-list --topo-order --branches --except merge > actual &&
	git rev-list --topo-order test > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list --except with full ref' '

	git rev-list --topo-order --branches --except refs/heads/merge > actual &&
	git rev-list --topo-order test > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list --except and --not' '

	git rev-list --topo-order test --not master --except master > actual &&
	git rev-list --topo-order test > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list --except and --not with proper flags' '

	git checkout -b maint master &&
	git checkout -b next test &&
	echo four > content &&
	git commit -a -m four &&
	git rev-list --topo-order next --not master maint --except maint > actual &&
	git rev-list --topo-order next --not master > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list --not ranges' '

	git rev-list --topo-order test --not master --except master test > actual &&
	git rev-list --topo-order test > expect &&
	test_cmp expect actual
'

test_expect_success 'rev-list multiple --not ranges' '

	git checkout -b extra test &&
	echo five > content &&
	git commit -a -m five &&
	git rev-list --topo-order test --not master --except master test --not extra > actual &&
	git rev-list --topo-order test extra > expect &&
	test_cmp expect actual
'

test_done
