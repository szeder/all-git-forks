#!/bin/sh

test_description='git rebase --pick-note'

. ./test-lib.sh

. "$TEST_DIRECTORY"/lib-rebase.sh

set_cat_todo_editor

test_expect_success setup '
	echo 1 >file &&
	git add . &&
	test_tick &&
	git commit -m "first commit" &&
	echo 2 >file &&
	git add . &&
	test_tick &&
	git commit -m "second commit" &&
	echo 3 >file &&
	git add . &&
	test_tick &&
	git commit -m "third commit"
'

test_expect_success without-pick-note '
	test_must_fail git rebase -i --root >actual &&
	cat <<-EOF >expected &&
	pick b1abfdb first commit
	pick aa0039e second commit
	pick 916f226 third commit
	EOF
	test_cmp expected actual
'

test_expect_success with-pick-note-short-option '
	test_must_fail git rebase \
		-A"echo shortsha1 must be \$shortsha1" \
		-i --root >actual
	cat <<-EOF >expected &&
	pick b1abfdb first commit
	shortsha1 must be b1abfdb
	pick aa0039e second commit
	shortsha1 must be aa0039e
	pick 916f226 third commit
	shortsha1 must be 916f226
	EOF
	test_cmp expected actual
'

test_expect_success with-pick-note-long-option '
	test_must_fail git rebase \
		--pick-note="echo shortsha1 must be \$shortsha1" \
		-i --root >actual
	cat <<-EOF >expected &&
	pick b1abfdb first commit
	shortsha1 must be b1abfdb
	pick aa0039e second commit
	shortsha1 must be aa0039e
	pick 916f226 third commit
	shortsha1 must be 916f226
	EOF
	test_cmp expected actual
'

test_expect_success with-pick-note-invalid-command '
	test_must_fail git rebase \
		-Afalse \
		-i --root 2>actual.in
	head -1 <actual.in >actual
	cat <<-EOF >expected &&
	--pick-note failed: false
	EOF
	test_cmp expected actual
'

test_expect_success with-pick-note-failed-after-second-pick '
	test_must_fail git rebase \
		--pick-note="if test x\$SHORT_SHA1 = xaa0039e; then false; fi" \
		-i --root 2>actual.in
	head -1 <actual.in >actual
	cat <<-EOF >expected &&
	--pick-note failed: if test x\$SHORT_SHA1 = xaa0039e; then false; fi
	EOF
	test_cmp expected actual
'

test_done
