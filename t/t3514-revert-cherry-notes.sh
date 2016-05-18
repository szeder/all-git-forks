#!/bin/sh

test_description='test cherry-pick and revert notes'

. ./test-lib.sh

test_expect_success 'notes are rewritten' '
	test_config notes.rewrite.cherry-pick true &&
	test_config notes.rewriteRef "refs/notes/*" &&
	test_commit n1 &&
	test_commit n2 &&
	git notes add -m "a note" n2 &&
	git checkout n1 &&
	git cherry-pick n2 &&
	git notes show HEAD
'

test_done
