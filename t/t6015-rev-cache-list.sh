#!/bin/sh

test_description='git rev-cache tests'
. ./test-lib.sh

sha1diff="python $TEST_DIRECTORY/t6015-sha1-dump-diff.py"

# we want a totally wacked out branch structure...
# we need branching and merging of sizes up through 3, tree 
# addition/deletion, and enough branching to exercise path 
# reuse
test_expect_success 'init repo' '
	echo bla >file && 
	git add . && 
	git commit -m "bla" && 

	git branch b1 && 
	git checkout b1 && 
	echo blu >file2 && 
	mkdir d1 && 
	echo bang >d1/filed1 && 
	git add . && 
	git commit -m "blu" && 
	
	git checkout master && 
	git branch b2 && 
	git checkout b2 && 
	echo kaplaa >>file && 
	git commit -a -m "kaplaa" && 
	
	git checkout master && 
	mkdir smoke && 
	echo omg >smoke/bong && 
	git add . && 
	git commit -m "omg" && 

	sleep 2 && 
	git branch b4 && 
	git checkout b4 && 
	echo shazam >file8 && 
	git add . && 
	git commit -m "shazam" && 
	git merge -m "merge b2" b2 && 
	
	echo bam >smoke/pipe && 
	git add . && 
	git commit -m "bam" && 

	git checkout master && 
	echo pow >file7 && 
	git add . && 
	git commit -m "pow" && 
	git merge -m "merge b4" b4 && 

	git checkout b1 && 
	echo stuff >d1/filed1 && 
	git commit -a -m "stuff" && 

	git branch b11 && 
	git checkout b11 && 
	echo wazzup >file3 &&
	git add file3 && 
	git commit -m "wazzup" && 

	git checkout b1 && 
	mkdir d1/d2 && 
	echo lol >d1/d2/filed2 && 
	git add . && 
	git commit -m "lol" && 

	sleep 2 && 
	git checkout master && 
	git merge -m "triple merge" b1 b11 && 
	git rm -r d1 &&  
	sleep 2 && 
	git commit -a -m "oh noes"
'

max_date=`git-rev-list --timestamp HEAD~1 --max-count=1 | grep -e "^[0-9]*" -o`
min_date=`git-rev-list --timestamp b4 --max-count=1 | grep -e "^[0-9]*" -o`

git-rev-list HEAD --not HEAD~3 >proper_commit_list_limited
git-rev-list HEAD >proper_commit_list
git-rev-list --objects HEAD >proper_object_list
git-rev-list HEAD --max-age=$min_date --min-age=$max_date >proper_list_date_limited

test_expect_success 'make cache slice' '
	mkdir .git/rev-cache && 
	git-rev-cache add HEAD 2>output.err && 
	grep "final return value: 0" output.err
'

test_expect_success 'remake cache slice' '
	git-rev-cache add --sizes HEAD 2>output.err && 
	grep "final return value: 0" output.err
'

#check core mechanics and rev-list hook for commits
test_expect_success 'test rev-caches walker directly (limited)' '
	git-rev-cache walk HEAD --not HEAD~3 >list && 
	cp list /cygdrive/c/list && 
	test -z `$sha1diff list proper_commit_list_limited`
'

test_expect_success 'test rev-caches walker directly (unlimited)' '
	git-rev-cache walk HEAD >list && 
	test -z `$sha1diff list proper_commit_list`
'

test_expect_success 'test rev-list traversal (limited)' '
	git-rev-list HEAD --not HEAD~3 >list && 
	cp list /cygdrive/c/list2 && 
	test -z `$sha1diff list proper_commit_list_limited`
'

test_expect_success 'test rev-list traversal (unlimited)' '
	git-rev-list HEAD >list && 
	test -z `$sha1diff list proper_commit_list`
'

#do the same for objects
test_expect_success 'test rev-caches walker with objects' '
	git-rev-cache walk --objects HEAD >list && 
	test -z `$sha1diff list proper_object_list`
'

test_expect_success 'test rev-list with objects (topo order)' '
	git-rev-list --topo-order --objects HEAD >list && 
	test -z `$sha1diff list proper_object_list`
'

test_expect_success 'test rev-list with objects (no order)' '
	git-rev-list --objects HEAD >list && 
	test -z `$sha1diff list proper_object_list`
'

#verify age limiting
test_expect_success 'test rev-list date limiting (topo order)' '
	git-rev-list --topo-order --max-age=$min_date --min-age=$max_date HEAD >list && 
	test -z `$sha1diff list proper_list_date_limited`
'

test_expect_success 'test rev-list date limiting (no order)' '
	git-rev-list --max-age=$min_date --min-age=$max_date HEAD >list && 
	test -z `sha1diff list proper_list_date_limited`
'

test_done

