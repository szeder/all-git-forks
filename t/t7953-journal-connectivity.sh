#!/bin/sh

test_description='check that journal-connectivity works'

. ./test-lib.sh

if test -z "$USE_LIBLMDB"; then
	skip_all='skipping test, git built without lmdb support'
	test_done
fi

test_expect_success 'set up for backfill test' '
	test_commit fleem &&
	git tag -a thetag -m tagmsg &&
	git checkout -b branch &&
	test_commit branch &&
	git checkout master &&
	test_commit premerge &&
	git merge branch &&
	git tag -d branch
'

test_expect_success 'journal-connectivity backfill works' '
	git backfill-journal-connectivity &&
	mdb_dump .git/connectivity-lmdb >data &&
	header_line=$(grep HEADER=END -n data|cut -d : -f 1) &&
	tail -n "+$header_line" data >actual &&

	# Generate our own "expect" data:

	# The commit at "fleem" is referenced four times: once each by
	# tags "fleem" and "thetag", once by the commit on "branch",
	# and once by the "premerge" commit.

	rec_commit=000003e8 &&
	rec_obj=000003e9 &&
	# It originates fleem.t and the corresponding tree
	objs=$(git rev-parse fleem^{tree} fleem:fleem.t | sort) &&
	printf "%s %s%s\n" $(git rev-parse fleem) $(echo ${rec_commit}0000000400000002) $(echo $objs | sed "s/ //g") >>expect-raw &&

	# The branch commit originates branch.t and its tree, and is referenced
	# by "branch" and "master"
	objs=$(git rev-parse branch^{tree} branch:branch.t fleem | sort) &&
	printf "%s %s%s\n" $(git rev-parse branch) $(echo ${rec_commit}0000000200000003) $(echo $objs | sed "s/ //g") >>expect-raw &&

	# The premerge commit originates premerge.t and its tree, and
	# is referenced by master and premerge
	objs=$(git rev-parse premerge^{tree} premerge:premerge.t fleem | sort) &&
	printf "%s %s%s\n" $(git rev-parse premerge) $(echo ${rec_commit}0000000200000003) $(echo $objs | sed "s/ //g") >>expect-raw &&

	# The merge commit originates just a tree (and its parents);
	# it is referenced only by master
	objs=$(git rev-parse master^{tree} premerge branch | sort) &&
	printf "%s %s%s\n" $(git rev-parse master) $(echo ${rec_commit}0000000100000003) $(echo $objs | sed "s/ //g") >>expect-raw &&

	# The first commit tree is referenced once (from the commit)
	printf "%s ${rec_obj}00000001\n" $(git rev-parse fleem^{tree}) >>expect-raw &&

	# The branch tree is referenced once
	printf "%s ${rec_obj}00000001\n" $(git rev-parse branch^{tree}) >>expect-raw &&

	# The premerge commit tree is referenced once
	printf "%s ${rec_obj}00000001\n" $(git rev-parse premerge^{tree}) >>expect-raw &&

	# The merge commit tree is referenced once
	printf "%s ${rec_obj}00000001\n" $(git rev-parse master^{tree}) >>expect-raw &&

	# The object containing "fleem" is referenced once (from the tree)
	printf "%s ${rec_obj}00000001\n" $(git rev-parse master:fleem.t) >>expect-raw &&

	# The object containing "branch" is referenced once (from the tree)
	printf "%s ${rec_obj}00000001\n" $(git rev-parse master:branch.t) >>expect-raw &&

	# The object containing "premerge" is referenced once (from the tree)
	printf "%s ${rec_obj}00000001\n" $(git rev-parse master:premerge.t) >>expect-raw &&

	# The tag object "thetag" is referenced once (from its ref)
	printf "%s ${rec_obj}00000001\n" $(git rev-parse thetag) >>expect-raw &&

	sort expect-raw >expect-sorted &&

	echo "HEADER=END" >expect &&
	tr " " "\n" <expect-sorted | sed "s/^/ /" >>expect &&
	echo "DATA=END" >>expect &&
	test_cmp expect actual
'

test_done
