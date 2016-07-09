#!/bin/sh

test_description='basic git gc tests
'

. ./test-lib.sh

test_expect_success 'gc empty repository' '
	git gc
'

test_expect_success 'gc does not leave behind pid file' '
	git gc &&
	test_path_is_missing .git/gc.pid
'

test_expect_success 'gc --gobbledegook' '
	test_expect_code 129 git gc --nonsense 2>err &&
	test_i18ngrep "[Uu]sage: git gc" err
'

test_expect_success 'gc -h with invalid configuration' '
	mkdir broken &&
	(
		cd broken &&
		git init &&
		echo "[gc] pruneexpire = CORRUPT" >>.git/config &&
		test_expect_code 129 git gc -h >usage 2>&1
	) &&
	test_i18ngrep "[Uu]sage" broken/usage
'

test_expect_success 'gc is not aborted due to a stale symref' '
	git init remote &&
	(
		cd remote &&
		test_commit initial &&
		git clone . ../client &&
		git branch -m develop &&
		cd ../client &&
		git fetch --prune &&
		git gc
	)
'

test_expect_success 'install pre-auto-gc hook for worktrees' '
	mkdir -p .git/hooks &&
	write_script .git/hooks/pre-auto-gc <<-\EOF
	echo "Preserving refs/reflogs of worktrees" >&2 &&
	dir="$(git rev-parse --git-common-dir)" &&
	refsdir="$dir/logs/refs" &&
	rm -f "$refsdir"/preserve &&
	ident="$(GIT_COMMITTER_DATE= git var GIT_COMMITTER_IDENT)" &&
	(
		find "$dir"/logs "$dir"/worktrees/*/logs \
			-type f -exec cat {} \; |
		cut -d" " -f1
		find "$dir"/HEAD "$dir"/worktrees/*/HEAD "$dir"/refs \
			"$dir"/worktrees/*/refs -type f -exec cat {} \; |
		grep -v "/^ref:/"
	) 2>/dev/null |
	sort |
	uniq |
	sed "s/.*/& & $ident	dummy/" >"$dir"/preserve &&
	mkdir -p "$refsdir" &&
	mv "$dir"/preserve "$refsdir"/
	EOF
'

trigger_auto_gc () {
	# This is unfortunately very, very ugly
	gdir="$(git rev-parse --git-common-dir)" &&
	mkdir -p "$gdir"/objects/17 &&
	touch "$gdir"/objects/17/17171717171717171717171717171717171717 &&
	touch "$gdir"/objects/17/17171717171717171717171717171717171718 &&
	git -c gc.auto=1 -c gc.pruneexpire=now -c gc.autodetach=0 gc --auto
}

test_expect_success 'gc respects refs/reflogs in all worktrees' '
	test_commit something &&
	git worktree add worktree &&
	(
		cd worktree &&
		git checkout --detach &&
		echo 1 >something.t &&
		test_tick &&
		git commit -m worktree-reflog something.t &&
		git rev-parse --verify HEAD >../commit-reflog &&
		echo 2 >something.t &&
		test_tick &&
		git commit -m worktree-ref something.t &&
		git rev-parse --verify HEAD >../commit-ref
	) &&
	trigger_auto_gc &&
	git rev-parse --verify $(cat commit-ref)^{commit} &&
	git rev-parse --verify $(cat commit-reflog)^{commit} &&

	# Now, add a reflog in the top-level dir and verify that `git gc` in
	# the worktree does not purge that
	git checkout --detach &&
	echo 3 >something.t &&
	test_tick &&
	git commit -m commondir-reflog something.t &&
	git rev-parse --verify HEAD >commondir-reflog &&
	(cd worktree && trigger_auto_gc) &&
	git rev-parse --verify $(cat commondir-reflog)^{commit}
'

test_done
