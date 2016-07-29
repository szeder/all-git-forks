#!/bin/sh
#
# Copyright (c) 2016 Google, Inc.
#

test_description='Test submodules with submodule.checkout setting

Historically the submodule.<name>.URL indicated the existence of the submodule,
this got decoupled, to have the submodule.checkout setting to indicate interest
in a submodule and the configured URL is only for overwriting the URL in
.gitmodules file, i.e. it may be missing in the config, but still have the
submodule checked out.'

. ./test-lib.sh


test_expect_success 'setup ' '
	git init sub &&
	(
		cd sub &&
		test_commit initial
	) &&
	git init super &&
	(
		cd super &&
		test_commit initial &&
		git submodule add ../sub sub1 &&
		git submodule add ../sub sub2 &&
		git commit -a -m "add 2 submodules at sub{1,2}"
	)
'

test_expect_success 'test submodule--helper should-exist' '
	(
		cd super &&
		git config --unset submodule.sub1.URL &&
		test_must_fail git submodule--helper should-exist sub1 &&
		git config submodule.sub1.URL ../sub &&
		git submodule--helper should-exist sub1 &&
		git config submodule.checkout "sub*" &&
		git submodule--helper should-exist sub1 &&
		git config --unset submodule.sub1.URL &&
		git submodule--helper should-exist sub1
	)
'

test_expect_success 'submodule add with new submodule.checkout option' '
	(
		cd super &&
		git submodule add ../sub sub3 &&
		git config --get-all submodule.checkout >actual &&
		echo "sub*" >expect &&
		test_cmp expect actual &&
		git submodule add ../sub anothersub &&
		git config --get-all submodule.checkout >actual &&
		echo "anothersub" >>expect &&
		test_cmp expect actual &&
		git commit -a -m "add sub3"
	)
'

cat <<-EOF >expect
-anothersub
 sub1 (initial)
-sub2
 sub3 (initial)
EOF


test_expect_success 'submodule status with new submodule.checkout option' '
	rm -rf super/sub2 &&
	rm -rf super/anothersub &&
	git config submodule.checkout "sub*" &&
	git -C super submodule status |cut -c1,43- >actual &&
	test_cmp expect actual
'

test_done
