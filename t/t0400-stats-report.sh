#!/bin/sh
#
# Copyright (c) 2015 Twitter, Inc.
#

test_description='git stats tests'

. ./test-lib.sh

START=`date +%s`

STATS_FILE=".gitstats/stats.log"

# skip test if not on OSX
if [ $(uname -s) != "Darwin" ]; then
  skip_all='skipping stats test; git was not compiled for OSX.'
  test_done
fi

otool -L  "${GIT_EXEC_PATH}/git" | grep -i breakpad &>/dev/null
compiled_with_breakpad=$?

if [ $compiled_with_breakpad -ne 0 ]; then
  skip_all='skipping stats tests; git was not compiled with breakpad.'
  test_done
fi

test_expect_success 'set up git repo' \
  'echo hell world >file1 &&
   echo goodbye people >file2 &&
   mkdir llamas &&
   echo "alpaca" > llamas/alpaca &&
   mkdir .gitstats &&
   touch .gitstats/lastupload &&
   git config --add twitter.statsenabled 1 &&
   git config --add twitter.statsurl http://bogus.hostname/is/a/bad/url &&
   rm ${STATS_FILE} &&
   git add .'

test_expect_success 'stats file exists' '
	test -f ${STATS_FILE}
'

test_expect_success 'stats file has git info' '
	test -f ${STATS_FILE} &&
	grep -q '\''"branch":"refs/heads/master"'\'' ${STATS_FILE} &&
	grep -q '\''"cmd":"add"'\'' ${STATS_FILE}
'

test_expect_success 'stats file has config' '
	grep -q '\''"key":"twitter.statsenabled"'\'' ${STATS_FILE}
'

test_expect_success 'stats file has timestamps' '
	TIMESTAMP=`egrep -o "timestamp.:[0-9]+" ${STATS_FILE} | sed -e "s/[^0-9]//g"`
	test ${TIMESTAMP} -ge ${START} &&
	test ${TIMESTAMP} -le `date +%s`
'

test_expect_success 'stats file has rusage' '
	egrep -q '\''"ru_oublock":[0-9]'\'' ${STATS_FILE}
'

test_expect_success 'bogus command is still recorded' '
	rm -rf .gitstats &&
	! git fonzbonz --intentionally-bogus &&
	test -f ${STATS_FILE} &&
	grep -q fonzbonz ${STATS_FILE}
'

test_expect_success 'git directory correctly reported' '
	rm -rf .gitstats &&
	(cd llamas && git status) &&
	grep -q '\''"path":"llamas"'\'' ${STATS_FILE}
'

test_expect_success 'unwritable stats file' '
	touch .gitstats/stats.log &&
	chmod 000 .gitstats/stats.log &&
	git status && # should have a positive exit status
	rm -rf .gitstats
'

test_done
