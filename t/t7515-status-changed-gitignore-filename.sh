#!/bin/sh

test_description='status with changed default .gitignore filename'
. ./test-lib.sh


#
# Check if only single file is in 'untracked' state: file2
# file1 is excluded by ignore pattern in .gitignore (default)
#
test_expect_success 'status with default .gitignore filename' '
	mkdir repo-default &&
	(cd repo-default && git init &&
	 echo "/file1" > .gitignore  &&
	 git add .gitignore &&
	 touch file1 &&
	 touch file2 &&
	 COUNT_UNTRACKED=`git status | grep -P "^\t" | grep -P -v "^\tnew file:" | grep -c .` &&
	 if [ "$COUNT_UNTRACKED" == "1" ]; then
	  true
	 else
	  false
	 fi
	)
'


#
# Check if only single file is in 'untracked' state: file2
# file1 is excluded by ignore pattern in .gitexclude (modified default ignore filename)
#
test_expect_success 'status with .gitignore filename reconfigured to .gitexclude' '
	mkdir repo-modified &&
	(cd repo-modified && git init &&
	 git config --add core.excludesperdirfilename .gitexclude &&
	 echo "/file1" > .gitexclude  &&
	 git add .gitexclude &&
	 touch file1 &&
	 touch file2 &&
	 COUNT_UNTRACKED=`git status | grep -P "^\t" | grep -P -v "^\tnew file:" | grep -c .` &&
	 if [ "$COUNT_UNTRACKED" == "1" ]; then
	  true
	 else
	  false
	 fi
	)
'


#
# Check if .gitignore is ignored when default exclude filename is changed
# file1 is excluded by ignore pattern in .gitexclude (modified default ignore filename)
#
test_expect_success 'ignore .gitignore when reconfigured to use .gitexclude' '
	mkdir repo-modified-failure &&
	(cd repo-modified-failure && git init &&
	 git config --add core.excludesperdirfilename .gitexclude &&
	 echo "/file1" > .gitignore  &&
	 git add .gitignore &&
	 touch file1 &&
	 touch file2 &&
	 COUNT_UNTRACKED=`git status | grep -P "^\t" | grep -P -v "^\tnew file:" | grep -c .` &&
	 if [ "$COUNT_UNTRACKED" == "2" ]; then
	  true
	 else
	  false
	 fi
	)
'


test_done
