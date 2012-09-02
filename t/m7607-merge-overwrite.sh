#!/bin/sh

#set -- -v -i

test_description='git-merge

Do not overwrite changes.'

. ./test-lib.sh

test_expect_success 'setup' '
	test_commit c1 c1.c &&
	test_commit c1a c1.c "c1 a" &&
	git checkout c1 &&
	git mv c1.c other.c &&
	test_tick &&
	git commit -m rename &&
	git tag rename &&
	echo "VERY IMPORTANT CHANGES" > important
'

test_expect_success 'will not overwrite unstaged changes in renamed file' '
	git reset --hard &&
	git checkout -f c1^{} &&
	test_commit c2 c2.c &&
	cp important other.c &&
	test_must_fail git merge rename &&
	test_cmp important other.c
'

test_expect_success 'will not overwrite unstaged changes in renamed file' '
	git reset --hard &&
	git checkout -f rename^{} &&
	cp important other.c &&
	test_might_fail git merge c1a &&
	test_cmp important other.c
'

test_expect_success 'will abort due to unstaged changes in renamed file' '
	git reset --hard &&
	git checkout -f rename^{} &&
	cp important other.c &&
	test_must_fail git merge c1a &&
	git diff --exit-code rename^{} &&
	test_cmp important other.c
'
	#gdb <&6 >&5 --args ../../git merge c1a &&

test_done
