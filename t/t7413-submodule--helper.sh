#!/bin/sh

# This should be merged with t7412 eventually.
# (currently in flight as jk/submodule-c-credential)


test_description='Basic plumbing support of submodule--helper

This test verifies the submodule--helper plumbing command used to implement
git-submodule.
'

. ./test-lib.sh

test_expect_success 'setup superproject with submodules' '

	mkdir sub &&
	(
		cd sub &&
		git init &&
		test_commit test
		test_commit test2
	) &&
	mkdir super &&
	(
		cd super &&
		git init &&
		git submodule add ../sub sub0 &&
		git submodule add -l bit1 ../sub sub1 &&
		git submodule add -l bit2 ../sub sub2 &&
		git submodule add -l bit2 -l bit1 ../sub sub3 &&
		git commit -m "add labeled submodules"
	)
'

test_expect_success 'in-group' '
	(
		cd super &&
		# we do not specify a group nor have set a default group,
		# any submodule should be in the default group:
		git submodule--helper in-group sub0 &&
		git submodule--helper in-group sub1 &&
		git submodule--helper in-group sub2 &&
		git submodule--helper in-group sub3 &&

		# test bit1:
		test_must_fail git submodule--helper in-group --group=*bit1 sub0 &&
			       git submodule--helper in-group --group=*bit1 sub1 &&
		test_must_fail git submodule--helper in-group --group=*bit1 sub2 &&
			       git submodule--helper in-group --group=*bit1 sub3 &&

		# test by path:
			       git submodule--helper in-group --group=./sub0 sub0 &&
		test_must_fail git submodule--helper in-group --group=./sub0 sub1 &&
		test_must_fail git submodule--helper in-group --group=./sub0 sub2 &&
		test_must_fail git submodule--helper in-group --group=./sub0 sub3 &&

		# test by name:
			       git submodule--helper in-group --group=:sub0 sub0 &&
		test_must_fail git submodule--helper in-group --group=:sub0 sub1 &&
		test_must_fail git submodule--helper in-group --group=:sub0 sub2 &&
		test_must_fail git submodule--helper in-group --group=:sub0 sub3 &&

		# logical OR of path and labels
			       git submodule--helper in-group --group=*bit1,./sub0 sub0 &&
			       git submodule--helper in-group --group=*bit1,./sub0 sub1 &&
		test_must_fail git submodule--helper in-group --group=*bit1,./sub0 sub2 &&
			       git submodule--helper in-group --group=*bit1,./sub0 sub3 &&

		# test if the config option is picked up
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&

			       git submodule--helper in-group sub0 &&
			       git submodule--helper in-group sub1 &&
		test_must_fail git submodule--helper in-group sub2 &&
			       git submodule--helper in-group sub3
	)
'

test_done
