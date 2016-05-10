#!/bin/sh

test_description='Test git update-ref with D/F conflicts'
. ./test-lib.sh

test_update_rejected () {
	prefix="$1" &&
	before="$2" &&
	pack="$3" &&
	create="$4" &&
	error="$5" &&
	printf "create $prefix/%s $C\n" $before |
	git update-ref --stdin &&
	git for-each-ref $prefix >unchanged &&
	if $pack
	then
		git pack-refs --all
	fi &&
	printf "create $prefix/%s $C\n" $create >input &&
	test_must_fail git update-ref --stdin <input 2>output.err &&
	grep -F "$error" output.err &&
	git for-each-ref $prefix >actual &&
	test_cmp unchanged actual
}

Q="'"

test_expect_success 'setup' '

	git commit --allow-empty -m Initial &&
	C=$(git rev-parse HEAD) &&
	git commit --allow-empty -m Second &&
	D=$(git rev-parse HEAD)

'

test_expect_success 'existing loose ref is a simple prefix of new' '

	prefix=refs/1l &&
	test_update_rejected $prefix "a c e" false "b c/x d" \
		"$Q$prefix/c$Q exists; cannot create $Q$prefix/c/x$Q"

'

test_expect_success 'existing packed ref is a simple prefix of new' '

	prefix=refs/1p &&
	test_update_rejected $prefix "a c e" true "b c/x d" \
		"$Q$prefix/c$Q exists; cannot create $Q$prefix/c/x$Q"

'

test_expect_success 'existing loose ref is a deeper prefix of new' '

	prefix=refs/2l &&
	test_update_rejected $prefix "a c e" false "b c/x/y d" \
		"$Q$prefix/c$Q exists; cannot create $Q$prefix/c/x/y$Q"

'

test_expect_success 'existing packed ref is a deeper prefix of new' '

	prefix=refs/2p &&
	test_update_rejected $prefix "a c e" true "b c/x/y d" \
		"$Q$prefix/c$Q exists; cannot create $Q$prefix/c/x/y$Q"

'

test_expect_success 'new ref is a simple prefix of existing loose' '

	prefix=refs/3l &&
	test_update_rejected $prefix "a c/x e" false "b c d" \
		"$Q$prefix/c/x$Q exists; cannot create $Q$prefix/c$Q"

'

test_expect_success 'new ref is a simple prefix of existing packed' '

	prefix=refs/3p &&
	test_update_rejected $prefix "a c/x e" true "b c d" \
		"$Q$prefix/c/x$Q exists; cannot create $Q$prefix/c$Q"

'

test_expect_success 'new ref is a deeper prefix of existing loose' '

	prefix=refs/4l &&
	test_update_rejected $prefix "a c/x/y e" false "b c d" \
		"$Q$prefix/c/x/y$Q exists; cannot create $Q$prefix/c$Q"

'

test_expect_success 'new ref is a deeper prefix of existing packed' '

	prefix=refs/4p &&
	test_update_rejected $prefix "a c/x/y e" true "b c d" \
		"$Q$prefix/c/x/y$Q exists; cannot create $Q$prefix/c$Q"

'

test_expect_success 'one new ref is a simple prefix of another' '

	prefix=refs/5 &&
	test_update_rejected $prefix "a e" false "b c c/x d" \
		"cannot process $Q$prefix/c$Q and $Q$prefix/c/x$Q at the same time"

'

test_expect_success 'empty directory should not fool rev-parse' '
	prefix=refs/e-rev-parse &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	echo "$C" >expected &&
	git rev-parse $prefix/foo >actual &&
	test_cmp expected actual
'

test_expect_success 'empty directory should not fool for-each-ref' '
	prefix=refs/e-for-each-ref &&
	git update-ref $prefix/foo $C &&
	git for-each-ref $prefix >expected &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	git for-each-ref $prefix >actual &&
	test_cmp expected actual
'

test_expect_success 'empty directory should not fool create' '
	prefix=refs/e-create &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "create %s $C\n" $prefix/foo |
	git update-ref --stdin
'

test_expect_success 'empty directory should not fool verify' '
	prefix=refs/e-verify &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "verify %s $C\n" $prefix/foo |
	git update-ref --stdin
'

test_expect_success 'empty directory should not fool 1-arg update' '
	prefix=refs/e-update-1 &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "update %s $D\n" $prefix/foo |
	git update-ref --stdin
'

test_expect_success 'empty directory should not fool 2-arg update' '
	prefix=refs/e-update-2 &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "update %s $D $C\n" $prefix/foo |
	git update-ref --stdin
'

test_expect_success 'empty directory should not fool 0-arg delete' '
	prefix=refs/e-delete-0 &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "delete %s\n" $prefix/foo |
	git update-ref --stdin
'

test_expect_success 'empty directory should not fool 1-arg delete' '
	prefix=refs/e-delete-1 &&
	git update-ref $prefix/foo $C &&
	git pack-refs --all &&
	mkdir -p .git/$prefix/foo/bar/baz &&
	printf "delete %s $C\n" $prefix/foo |
	git update-ref --stdin
'

test_done
