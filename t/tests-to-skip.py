#!/usr/bin/env python2.7

from __future__ import print_function

import string
import sys

datatxt = '''

########################################################################
##
## Below is an annotated list of tests to skip.
##
## Please don't add tests to this list without a clear comment, preferably naming a JIRA ticket.
##
## The format of each line is "<test><sp><tag>[!][<sp><tag>[!]]..." where <test> is a git test
## identifier suitable for inclusion in the GIT_SKIP_TESTS environment, and <tag> is an arbitary
## alphanumeric string used for matching. A '!' suffix indicates "mandatory". You can run this
## script to produce a filtered list of tests suitable for direct assignment to the environment
## GIT_SKIP_TESTS. Pass as positional parameters expressions in the form <tag>[+<tag>]...
## For EACH expression, tests are matched which have ALL of the tags in the expression, EXCEPT
## for those tests having "mandatory" tags which are NOT in the expression. The meaning of tags
## is defined by the consumer, the seminal one being 'run-tests.sh' in this directory.
##

#XXX SRC-826 - test t0400 is flaky
t0400	Linux Darwin

#XXX tests fail under watchman+prove
t0000	watchman! Linux Darwin
t3000	watchman! Linux Darwin
t3010	watchman! Linux Darwin
t3910	watchman! Linux Darwin
t4004	watchman! Linux Darwin
t4008	watchman! Linux Darwin
t4011	watchman! Linux Darwin
t4020	watchman! Linux Darwin
t4122	watchman! Linux Darwin
t6039	watchman! Linux Darwin
t7400	watchman! Linux Darwin
t7506	watchman! Linux Darwin
t7508	watchman! Linux Darwin

# SRC-1072 -- fix these then re-add them
t4023 watchman! Darwin
t4201 watchman! Darwin
t6038 watchman! Darwin

########################################################################

'''

opt_tags = dict()
man_tags = dict()
tests = set()
line_gen = (line.split() for line in datatxt.splitlines() if line != '' and not line.startswith('#'))
for test, tags in ((l[0], l[1:]) for l in line_gen):
  tests.add(test)
  opt_tags[test] = set((tag for tag in tags if not tag.endswith('!')))
  man_tags[test] = set((tag[:-1] for tag in tags if tag.endswith('!')))

out = set()
for spec in sys.argv[1:]:
  search = set(spec.split('+'))
  for test in tests:
    if (opt_tags[test] | man_tags[test] >= search) and (search >= man_tags[test]):
      out.add(test)

print(' '.join(sorted(out)))

