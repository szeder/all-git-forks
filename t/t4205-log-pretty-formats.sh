#!/bin/sh
#
# Copyright (c) 2010, Will Palmer
# Copyright (c) 2011, Alexey Shumkin (+ non-UTF-8 commit encoding tests)
#

test_description='Test pretty formats'
. ./test-lib.sh

commit_msg() {
	# String "initial commit" partly in Russian, encoded in UTF-8,
	# used as a commit log message below.
	msg=$(printf "initial \320\272\320\276\320\274\320\274\320\270\321\202")
	if test -n "$1"; then
		msg=$(echo $msg | iconv -f utf-8 -t $1)
	fi
	echo $msg
}

test_expect_success 'set up basic repos' '
	>foo &&
	>bar &&
	git add foo &&
	test_tick &&
	git config i18n.commitEncoding cp1251 &&
	git commit -m "$(commit_msg cp1251)" &&
	git add bar &&
	test_tick &&
	git commit -m "add bar" &&
	git config --unset i18n.commitEncoding
'

test_expect_success 'alias builtin format' '
	git log --pretty=oneline >expected &&
	git config pretty.test-alias oneline &&
	git log --pretty=test-alias >actual &&
	test_cmp expected actual
'

test_expect_success 'alias masking builtin format' '
	git log --pretty=oneline >expected &&
	git config pretty.oneline "%H" &&
	git log --pretty=oneline >actual &&
	test_cmp expected actual
'

test_expect_success 'alias user-defined format' '
	git log --pretty="format:%h" >expected &&
	git config pretty.test-alias "format:%h" &&
	git log --pretty=test-alias >actual &&
	test_cmp expected actual
'

test_expect_success 'alias user-defined tformat with %s (cp1251 encoding)' '
	git config i18n.logOutputEncoding cp1251 &&
	git log --oneline >expected-s &&
	git log --pretty="tformat:%h %s" >actual-s &&
	git config --unset i18n.logOutputEncoding &&
	test_cmp expected-s actual-s
'

test_expect_success 'alias user-defined tformat with %s (utf-8 encoding)' '
	git log --oneline >expected-s &&
	git log --pretty="tformat:%h %s" >actual-s &&
	test_cmp expected-s actual-s
'

test_expect_success 'alias user-defined tformat' '
	git log --pretty="tformat:%h" >expected &&
	git config pretty.test-alias "tformat:%h" &&
	git log --pretty=test-alias >actual &&
	test_cmp expected actual
'

test_expect_success 'alias non-existent format' '
	git config pretty.test-alias format-that-will-never-exist &&
	test_must_fail git log --pretty=test-alias
'

test_expect_success 'alias of an alias' '
	git log --pretty="tformat:%h" >expected &&
	git config pretty.test-foo "tformat:%h" &&
	git config pretty.test-bar test-foo &&
	git log --pretty=test-bar >actual && test_cmp expected actual
'

test_expect_success 'alias masking an alias' '
	git log --pretty=format:"Two %H" >expected &&
	git config pretty.duplicate "format:One %H" &&
	git config --add pretty.duplicate "format:Two %H" &&
	git log --pretty=duplicate >actual &&
	test_cmp expected actual
'

test_expect_success 'alias loop' '
	git config pretty.test-foo test-bar &&
	git config pretty.test-bar test-foo &&
	test_must_fail git log --pretty=test-foo
'

test_expect_success 'NUL separation' '
	printf "add bar\0$(commit_msg)" >expected &&
	git log -z --pretty="format:%s" >actual &&
	test_cmp expected actual
'

test_expect_success 'NUL termination' '
	printf "add bar\0$(commit_msg)\0" >expected &&
	git log -z --pretty="tformat:%s" >actual &&
	test_cmp expected actual
'

test_expect_success 'NUL separation with --stat' '
	stat0_part=$(git diff --stat HEAD^ HEAD) &&
	stat1_part=$(git diff-tree --no-commit-id --stat --root HEAD^) &&
	printf "add bar\n$stat0_part\n\0$(commit_msg)\n$stat1_part\n" >expected &&
	git log -z --stat --pretty="format:%s" >actual &&
	test_i18ncmp expected actual
'

test_expect_failure 'NUL termination with --stat' '
	stat0_part=$(git diff --stat HEAD^ HEAD) &&
	stat1_part=$(git diff-tree --no-commit-id --stat --root HEAD^) &&
	printf "add bar\n$stat0_part\n\0$(commit_msg)\n$stat1_part\n0" >expected &&
	git log -z --stat --pretty="tformat:%s" >actual &&
	test_i18ncmp expected actual
'

test_done
