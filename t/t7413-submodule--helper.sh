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

submodule_sha1=$(git -C sub rev-parse HEAD)

cat >expect <<-EOF
160000 $submodule_sha1 0	sub0
160000 $submodule_sha1 0	sub1
160000 $submodule_sha1 0	sub3
EOF

test_expect_success 'submodule--helper list respects groups' '
	(
		cd super &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule--helper list >../actual
	) &&
	test_cmp expect actual
'

cat >expect <<-EOF
Entering 'sub0'
$submodule_sha1 sub0
Entering 'sub1'
$submodule_sha1 sub1
Entering 'sub3'
$submodule_sha1 sub3
EOF

test_expect_success 'submodule foreach respects groups' '
	(
		cd super &&
		git submodule foreach "echo \$sha1 \$name" >../actual
	) &&
	test_cmp expect actual
'

sub_priorsha1=$(git -C sub rev-parse HEAD^)

cat >expect <<-EOF
+$sub_priorsha1 sub0 (test)
+$sub_priorsha1 sub1 (test)
+$sub_priorsha1 sub3 (test)
EOF

test_expect_success 'submodule status respects groups' '
	git clone --recurse-submodules super super_clone &&
	(
		cd super_clone &&
		git -C sub0 checkout HEAD^ &&
		git -C sub1 checkout HEAD^ &&
		git -C sub2 checkout HEAD^ &&
		git -C sub3 checkout HEAD^ &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule status >../actual &&
		git config --unset-all submodule.defaultGroup &&
		git submodule update
	) &&
	test_cmp expect actual
'

test_expect_success 'submodule deinit respects groups' '
	suburl=$(pwd)/sub &&
	(
		cd super_clone &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule deinit &&
		test_must_fail git config submodule.sub0.url &&
		test_must_fail git config submodule.sub1.url &&
		test "$(git config submodule.sub2.url)" = "$suburl" &&
		test_must_fail git config submodule.sub3.url &&
		git config --unset-all submodule.defaultGroup &&
		git submodule init
	)
'

test_expect_success 'submodule sync respects groups' '
	suburl=$(pwd)/sub &&
	(
		cd super_clone &&
		git config submodule.sub0.url nonsense &&
		git config submodule.sub1.url nonsense &&
		git config submodule.sub2.url nonsense &&
		git config submodule.sub3.url nonsense &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule sync &&
		git config --unset-all submodule.defaultGroup &&
		test "$(git config submodule.sub0.url)" = "$suburl" &&
		test "$(git config submodule.sub1.url)" = "$suburl" &&
		test "$(git config submodule.sub2.url)" = "nonsense" &&
		test "$(git config submodule.sub3.url)" = "$suburl" &&
		git submodule sync sub2 &&
		test "$(git config submodule.sub2.url)" = "$suburl"
	)
'

test_expect_success 'submodule--helper init respects groups' '
	(
		cd super_clone &&
		git submodule deinit . &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule init &&
		git config --unset-all submodule.defaultGroup &&
		test "$(git config submodule.sub0.url)" = "$suburl" &&
		test "$(git config submodule.sub1.url)" = "$suburl" &&
		test_must_fail git config submodule.sub2.url &&
		test "$(git config submodule.sub3.url)" = "$suburl"
	)
'

cat >expect <<-EOF
160000 $submodule_sha1 0 1	sub0
160000 $submodule_sha1 0 1	sub1
160000 $submodule_sha1 0 1	sub3
EOF

test_expect_success 'submodule--helper update-clone respects groups' '
	(
		cd super_clone &&
		git submodule init &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule--helper update-clone >../actual &&
		git config --unset-all submodule.defaultGroup
	) &&
	test_cmp expect actual
'

cat >expect <<-EOF
Submodule path 'sub0': checked out '$submodule_sha1'
Submodule path 'sub1': checked out '$submodule_sha1'
Submodule path 'sub3': checked out '$submodule_sha1'
EOF

test_expect_success 'git submodule update respects groups' '
	(
		cd super_clone &&
		git submodule deinit -f . &&
		git config --add submodule.defaultGroup *bit1 &&
		git config --add submodule.defaultGroup ./sub0 &&
		git submodule update --init >../actual &&
		git config --unset-all submodule.defaultGroup
	) &&
	test_cmp expect actual
'

test_done
