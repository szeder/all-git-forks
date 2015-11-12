#!/bin/sh

test_description='Test shallow cloning of repos with submodules'

. ./test-lib.sh

test_expect_success setup '
	git checkout -b master &&
	echo file >file &&
	git add file &&
	test_tick &&
	git commit -m "master commit 1" &&

	git checkout -b branch &&
	echo file >branch-file &&
	git add branch-file &&
	test_tick &&
	git commit -m "branch commit 1" &&

	git checkout master &&
	git clone . repo &&
	(
		cd repo &&
		git checkout master &&
		git submodule add ../. submodule &&
		(
			cd submodule &&
			git checkout branch
		) &&
		git add submodule &&
		test_tick &&
		git commit -m "master commit 2"
	)
'

test_expect_failure shallow-clone-recursive '
	URL="file://$(pwd | sed "s/[[:space:]]/%20/g")/repo" &&
	echo $URL &&
	git clone --recursive --depth 1 --single-branch $URL clone-recursive &&
	(
		cd "clone-recursive" &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	) &&
	(
		cd "clone-recursive/submodule" &&
		git log --oneline >lines &&
		test_line_count = 1 lines
	)
'

test_done
