#!/bin/sh

test_description='checkout handling of ambiguous (branch/tag) refs'
. ./test-lib.sh

test_expect_success 'setup ambiguous refs' '
	test_commit branch file &&
	git branch ambiguity &&
	git branch vagueness &&
	test_commit tag file &&
	git tag ambiguity &&
	git tag vagueness HEAD:file &&
	test_commit other file
'

test_expect_success 'checkout ambiguous ref succeeds' '
	git checkout ambiguity >stdout 2>stderr
'

test_expect_success 'checkout produces ambiguity warning' '
	grep "warning.*ambiguous" stderr
'

test_expect_success 'checkout chooses branch over tag' '
	echo branch >expect &&
	test_cmp expect file
'

test_expect_success 'checkout reports switch to detached HEAD' '
	grep "Switched to branch" stderr &&
	! grep "^HEAD is now at" stderr
'

test_expect_failure 'checkout vague ref succeeds' '
	git checkout vagueness >stdout 2>stderr &&
	test_set_prereq VAGUENESS_SUCCESS
'

test_expect_success VAGUENESS_SUCCESS 'checkout produces ambiguity warning' '
	grep "warning.*ambiguous" stderr
'

test_expect_success VAGUENESS_SUCCESS 'checkout chooses branch over tag' '
	echo branch >expect &&
	test_cmp expect file
'

test_expect_success VAGUENESS_SUCCESS 'checkout reports switch to detached HEAD' '
	grep "Switched to branch" stderr &&
	! grep "^HEAD is now at" stderr
'

test_done
