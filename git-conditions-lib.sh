#!/bin/sh
#
# (c) Copyright Jon Seymour 2010
#
if test -z "${__GIT_CONDITIONS_LIB_INCLUDED}"
then
__GIT_CONDITIONS_LIB_INCLUDED=t
#
# Provides a function that enables the evaluation of assertions.
#
check_unstaged_0()
{
	if test $(git diff-files --name-only | wc -l) -ne 0
	then
		echo "There are unstaged files."
	else
		echo "There are no unstaged files."
		false
	fi
}

check_staged_0()
{
	if test $(git diff-index --cached --name-only HEAD | wc -l) -ne 0
	then
		echo "There are staged files."
	else
		echo "There are no staged files."
		false
	fi
}

check_untracked_0()
{
	if test $(git ls-files -o --exclude-standard | wc -l) -ne 0
	then
		echo "There are untracked files."
	else
		echo "There are no untracked files."
		false
	fi
}

check_conflicted_0()
{
	if test $(git ls-files --unmerged | wc -l) -ne 0
	then
		echo "There are unmerged files."
	else
		echo "There are no unmerged files."
		false
	fi

}


fi
