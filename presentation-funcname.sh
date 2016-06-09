#!/bin/sh
#
# Copyright (c) 2007 Johannes E. Schindelin
#

test_description='Test custom diff function name patterns'
cd t
. ./test-lib.sh

	# a non-trivial custom pattern
	git config diff.custom1.funcname "!static
!String
[^ 	].*s.*"

diffpatterns="
	css
"

for i in $diffpatterns
do
    echo "$i-* diff=$i"
done > .gitattributes &&

# add all test files to the index
(
    cd "$TEST_DIRECTORY"/t4018 &&
    git --git-dir="$TRASH_DIRECTORY/.git" add .
) &&

# place modified files in the worktree
for i in $(git ls-files)
do
    sed -e "s/FindContext/ContextFound/" <"$TEST_DIRECTORY/t4018/$i" >"$i" ||
    return 1
done

# check each individual file
for i in $(git ls-files css-soutenance)
do
	if grep broken "$i" >/dev/null 2>&1
	then
		result=failure
	else
		result=success
	fi
	cat $TEST_DIRECTORY/t4018/$i && echo &&
	echo "------------------" &&
	echo "------------------" && read pause  &&
		test_when_finished 'cat actual' &&	# for debugging only
		git diff -U1 $i
done

test_done
