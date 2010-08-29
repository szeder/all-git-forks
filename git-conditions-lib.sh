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

check_rebasing_0()
{
	if test -d "$(git rev-parse --git-dir)/rebase-apply"
	then
		echo "A rebase is in progress."
	else
		echo "There is no rebase in progress."
		false
	fi
}

check_detached_0()
{
	if ! git symbolic-ref -q HEAD >/dev/null
	then
		echo "HEAD is detached."
	else
		echo "HEAD is not detached."
		false
	fi
}

check_branch_exists_1()
{
	symbolic=$(git rev-parse --quiet --symbolic-full-name --verify "$1")
 	if test "${symbolic#refs/heads/}" != "${symbolic}"
	then
		echo "Branch '$1' exists."
	else
		echo "Branch '$1' does not exist."
		false
	fi
}

check_tag_exists_1()
{
	symbolic=$(git rev-parse --quiet --symbolic-full-name --verify "$1")
 	if test "${symbolic#refs/tags/}" != "${symbolic}"
	then
		echo "Tag '$1' exists."
	else
		echo "Tag '$1' does not exist."
		false
	fi
}

check_ref_exists_1()
{
	symbolic=$(git rev-parse --quiet --symbolic-full-name --verify "$1")
 	if test "${symbolic#refs/}" != "${symbolic}"
	then
		echo "Reference '$1' exists."
	else
		echo "Reference '$1' does not exist."
		false
	fi
}


fi
