#!/bin/sh

test_description='git rebase --rewrite-refs'

. ./test-lib.sh
. "$TEST_DIRECTORY/lib-rebase.sh"
set_fake_editor

#      part1  part2 topic
#	 |	|     |
#	 v	v     v
#  A-----C------D-----E
#   \
#    B <--master

test_expect_success setup '
	test_commit A &&
	git branch topic &&
	test_commit B &&
	git checkout topic &&
	test_commit C &&
	git branch part1 &&
	test_commit D &&
	git branch part2 &&
	test_commit E
'

test_expect_success 'rebase --rewrite-heads' '
	git reset --hard &&
	git checkout topic &&
	git reset --hard E &&

	git rebase --rewrite-heads master &&
	git rev-parse part1 >actual &&
	git rev-parse HEAD~2 >expected &&
	test_cmp expected actual &&
	git rev-parse part2 >actual &&
	git rev-parse HEAD~1 >expected &&
	test_cmp expected actual
'

test_expect_success 'rebase --rewrite-refs' '
	git reset --hard &&
	git update-ref refs/heads/part1 C &&
	git update-ref refs/heads/part2 D &&
	git checkout topic &&
	git reset --hard E &&

	git rebase --rewrite-refs=refs/heads/part2 master &&
	git rev-parse part1 >actual &&
	git rev-parse C >expected &&
	test_cmp expected actual &&
	git rev-parse part2 >actual &&
	git rev-parse HEAD~1 >expected &&
	test_cmp expected actual
'

test_expect_success 'ref in TODO followed by rebase --abort' '
	git reset --hard &&
	git update-ref refs/heads/part1 C &&
	git update-ref refs/heads/part2 D &&
	git checkout topic &&
	git reset --hard E &&

	FAKE_LINES="1 2 edit 3 4 5" git rebase -i --rewrite-heads master &&
	git rev-parse part1 >actual &&
	git rev-parse HEAD^ >expected &&
	test_cmp expected actual &&
	git rebase --abort &&
	git rev-parse part1 >actual &&
	git rev-parse C >expected &&
	test_cmp expected actual
'

#        partX topicX
#          |     |
#          v     v
#  A---C---B'----F
#   \
#    B <--master

test_expect_success 'rewrite ref pointing to commit dropped as dupe' '
	git checkout -b topicX &&
	git reset --hard C &&
	git cherry-pick B &&
	git branch partX &&
	test_commit F &&

	git rebase --rewrite-heads master &&
	git reset --hard &&
	git rev-parse HEAD~2 >actual &&
	git rev-parse master >expected &&
	test_cmp expected actual &&
	git rev-parse partX >actual &&
	git rev-parse HEAD~1 >expected &&
	test_cmp expected actual
'

test_done
