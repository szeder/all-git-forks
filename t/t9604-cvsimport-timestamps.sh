#!/bin/sh

test_description='git cvsimport timestamps'
. ./lib-cvs.sh

setup_cvs_test_repository t9604

test_expect_success 'check timestamps are UTC (TZ=America/Chicago)' '
	TZ=America/Chicago git cvsimport -p"-x" -C module-1 module &&
	git cvsimport -p"-x" -C module-1 module &&
	(
		cd module-1 &&
		git log --format="%s %ai"
	)  >actual-1 &&
	cat >expect-1 <<-\EOF &&
	Rev 16 2011-11-06 07:00:01 +0000
	Rev 15 2011-11-06 06:59:59 +0000
	Rev 14 2011-03-13 08:00:01 +0000
	Rev 13 2011-03-13 07:59:59 +0000
	Rev 12 2010-12-01 00:00:00 +0000
	Rev 11 2010-11-01 00:00:00 +0000
	Rev 10 2010-10-01 00:00:00 +0000
	Rev  9 2010-09-01 00:00:00 +0000
	Rev  8 2010-08-01 00:00:00 +0000
	Rev  7 2010-07-01 00:00:00 +0000
	Rev  6 2010-06-01 00:00:00 +0000
	Rev  5 2010-05-01 00:00:00 +0000
	Rev  4 2010-04-01 00:00:00 +0000
	Rev  3 2010-03-01 00:00:00 +0000
	Rev  2 2010-02-01 00:00:00 +0000
	Rev  1 2010-01-01 00:00:00 +0000
	EOF
	test_cmp actual-1 expect-1
'

test_expect_success 'check timestamps are UTC (TZ=Australia/Sydney)' '
	TZ=America/Chicago git cvsimport -p"-x" -C module-2 module &&
	git cvsimport -p"-x" -C module-2 module &&
	(
		cd module-2 &&
		git log --format="%s %ai"
	) >actual-2 &&
	cat >expect-2 <<-\EOF &&
	Rev 16 2011-11-06 07:00:01 +0000
	Rev 15 2011-11-06 06:59:59 +0000
	Rev 14 2011-03-13 08:00:01 +0000
	Rev 13 2011-03-13 07:59:59 +0000
	Rev 12 2010-12-01 00:00:00 +0000
	Rev 11 2010-11-01 00:00:00 +0000
	Rev 10 2010-10-01 00:00:00 +0000
	Rev  9 2010-09-01 00:00:00 +0000
	Rev  8 2010-08-01 00:00:00 +0000
	Rev  7 2010-07-01 00:00:00 +0000
	Rev  6 2010-06-01 00:00:00 +0000
	Rev  5 2010-05-01 00:00:00 +0000
	Rev  4 2010-04-01 00:00:00 +0000
	Rev  3 2010-03-01 00:00:00 +0000
	Rev  2 2010-02-01 00:00:00 +0000
	Rev  1 2010-01-01 00:00:00 +0000
	EOF
	test_cmp actual-2 expect-2
'

test_expect_success 'check timestamps with author-specific timezones' '
	cat >cvs-authors <<-EOF &&
	user1=User One <user1@domain.org>
	user2=User Two <user2@domain.org> America/Chicago
	user3=User Three <user3@domain.org> Australia/Sydney
	user4=User Four <user4@domain.org> Asia/Shanghai
	EOF
	git cvsimport -p"-x" -A cvs-authors -C module-3 module &&
	(
		cd module-3 &&
		git log --format="%s %ai %an"
	) >actual-3 &&
	cat >expect-3 <<-\EOF
	Rev 16 2011-11-06 01:00:01 -0600 User Two
	Rev 15 2011-11-06 01:59:59 -0500 User Two
	Rev 14 2011-03-13 03:00:01 -0500 User Two
	Rev 13 2011-03-13 01:59:59 -0600 User Two
	Rev 12 2010-12-01 08:00:00 +0800 User Four
	Rev 11 2010-11-01 11:00:00 +1100 User Three
	Rev 10 2010-09-30 19:00:00 -0500 User Two
	Rev  9 2010-09-01 00:00:00 +0000 User One
	Rev  8 2010-08-01 08:00:00 +0800 User Four
	Rev  7 2010-07-01 10:00:00 +1000 User Three
	Rev  6 2010-05-31 19:00:00 -0500 User Two
	Rev  5 2010-05-01 00:00:00 +0000 User One
	Rev  4 2010-04-01 08:00:00 +0800 User Four
	Rev  3 2010-03-01 11:00:00 +1100 User Three
	Rev  2 2010-01-31 18:00:00 -0600 User Two
	Rev  1 2010-01-01 00:00:00 +0000 User One
	EOF
	test_cmp actual-3 expect-3
'

test_done
