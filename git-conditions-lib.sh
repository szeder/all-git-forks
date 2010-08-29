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


fi
