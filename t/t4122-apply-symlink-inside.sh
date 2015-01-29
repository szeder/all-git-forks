#!/bin/sh

test_description='apply to deeper directory without getting fooled with symlink'
. ./test-lib.sh

lecho () {
	for l_
	do
		echo "$l_"
	done
}

test_expect_success setup '

	mkdir -p arch/i386/boot arch/x86_64 &&
	lecho 1 2 3 4 5 >arch/i386/boot/Makefile &&
	test_ln_s_add ../i386/boot arch/x86_64/boot &&
	git add . &&
	test_tick &&
	git commit -m initial &&
	git branch test &&

	rm arch/x86_64/boot &&
	mkdir arch/x86_64/boot &&
	lecho 2 3 4 5 6 >arch/x86_64/boot/Makefile &&
	git add . &&
	test_tick &&
	git commit -a -m second &&

	git format-patch --binary -1 --stdout >test.patch

'

test_expect_success apply '

	git checkout test &&
	git diff --exit-code test &&
	git diff --exit-code --cached test &&
	git apply --index test.patch

'

test_expect_success 'check result' '

	git diff --exit-code master &&
	git diff --exit-code --cached master &&
	test_tick &&
	git commit -m replay &&
	T1=$(git rev-parse "master^{tree}") &&
	T2=$(git rev-parse "HEAD^{tree}") &&
	test "z$T1" = "z$T2"

'

test_expect_success SYMLINKS 'do not follow symbolic link (setup)' '

	git reset --hard &&
	ln -s ../i386/dir arch/x86_64/dir &&
	git add arch/x86_64/dir &&
	git diff HEAD >add_symlink.patch &&
	git reset --hard &&

	mkdir arch/x86_64/dir &&
	>arch/x86_64/dir/file &&
	git add arch/x86_64/dir/file &&
	git diff HEAD >add_file.patch &&
	git reset --hard &&
	rm -fr arch/x86_64/dir &&

	cat add_symlink.patch add_file.patch >patch &&

	mkdir arch/i386/dir
'

test_expect_success SYMLINKS 'do not follow symbolic link (same input)' '

	# same input creates a confusing symbolic link
	test_must_fail git apply patch 2>error-wt &&
	test_i18ngrep "beyond a symbolic link" error-wt &&
	test ! -e arch/x86_64/dir &&
	test ! -e arch/i386/dir/file &&

	test_must_fail git apply --index patch 2>error-ix &&
	test_i18ngrep "beyond a symbolic link" error-ix &&
	test ! -e arch/x86_64/dir &&
	test ! -e arch/i386/dir/file &&
	test_must_fail git ls-files --error-unmatch arch/x86_64/dir &&
	test_must_fail git ls-files --error-unmatch arch/i386/dir &&

	test_must_fail git apply --cached patch 2>error-ct &&
	test_i18ngrep "beyond a symbolic link" error-ct &&
	test_must_fail git ls-files --error-unmatch arch/x86_64/dir &&
	test_must_fail git ls-files --error-unmatch arch/i386/dir
'

test_expect_success SYMLINKS 'do not follow symbolic link (existing)' '

	# existing symbolic link
	git reset --hard &&
	ln -s ../i386/dir arch/x86_64/dir &&
	git add arch/x86_64/dir &&

	test_must_fail git apply add_file.patch 2>error-wt-file &&
	test_i18ngrep "beyond a symbolic link" error-wt-file &&
	test ! -e arch/i386/dir/file &&

	test_must_fail git apply --index add_file.patch 2>error-ix-file &&
	test_i18ngrep "beyond a symbolic link" error-ix-file &&
	test ! -e arch/i386/dir/file &&
	test_must_fail git ls-files --error-unmatch arch/i386/dir &&

	test_must_fail git apply --cached add_file.patch 2>error-ct-file &&
	test_i18ngrep "beyond a symbolic link" error-ct-file &&
	test_must_fail git ls-files --error-unmatch arch/i386/dir
'

test_done
