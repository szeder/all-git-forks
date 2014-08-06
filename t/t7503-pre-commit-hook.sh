#!/bin/sh

test_description='pre-commit hook'

. ./test-lib.sh

test_expect_success 'with no hook' '

	echo "foo" > file &&
	git add file &&
	git commit -m "first"

'

test_expect_success '--no-pre-commit with no hook' '

	echo "bar" > file &&
	git add file &&
	git commit --no-pre-commit -m "bar"

'

# now install hook that always succeeds
HOOKDIR="$(git rev-parse --git-dir)/hooks"
HOOK="$HOOKDIR/pre-commit"
mkdir -p "$HOOKDIR"
cat > "$HOOK" <<EOF
#!/bin/sh
exit 0
EOF
chmod +x "$HOOK"

test_expect_success 'with succeeding hook' '

	echo "more" >> file &&
	git add file &&
	git commit -m "more"

'

test_expect_success '--no-pre-commit with succeeding hook' '

	echo "even more" >> file &&
	git add file &&
	git commit --no-pre-commit -m "even more"

'

# now a hook that fails
cat > "$HOOK" <<EOF
#!/bin/sh
exit 1
EOF

test_expect_success 'with failing hook' '

	echo "another" >> file &&
	git add file &&
	test_must_fail git commit -m "another"

'

test_expect_success '--no-pre-commit with failing hook' '

	echo "stuff" >> file &&
	git add file &&
	git commit --no-pre-commit -m "stuff"

'

chmod -x "$HOOK"
test_expect_success POSIXPERM 'with non-executable hook' '

	echo "content" >> file &&
	git add file &&
	git commit -m "content"

'

test_expect_success POSIXPERM '--no-pre-commit with non-executable hook' '

	echo "more content" >> file &&
	git add file &&
	git commit --no-pre-commit -m "more content"

'
chmod +x "$HOOK"

test_hook_enabled () {
	git checkout --detach master &&
	output="running failing pre-commit hook with ${*:-(none)}..." &&
	>actual.output &&
	cat >"$HOOK" <<-EOF &&
	#!/bin/sh
	echo "$output" >>actual.output
	exit 1
	EOF
	chmod +x "$HOOK" &&
	echo "$output" >expected.output &&
	test_must_fail test_commit $* file &&
	test_cmp expected.output actual.output
}

test_hook_disabled () {
	git checkout --detach master &&
	output="running failing pre-commit hook with ${*:-(none)}..." &&
	>actual.output &&
	cat >"$HOOK" <<-EOF &&
	#!/bin/sh
	echo "$output" >>actual.output
	exit 1
	EOF
	chmod +x "$HOOK" &&
	test_commit --notag $* file &&
	test_must_be_empty actual.output
}

test_expect_success 'command line options combinations' '
	test_hook_enabled &&
	test_hook_enabled --pre-commit &&
	test_hook_enabled --no-pre-commit --pre-commit &&
	test_hook_enabled --no-verify --pre-commit &&
	test_hook_enabled --verify &&
	test_hook_enabled --no-pre-commit --verify &&
	test_hook_enabled --no-verify --verify &&
	test_hook_enabled --verify --no-commit-msg &&
	test_hook_enabled --verify --commit-msg &&
	test_hook_disabled --no-pre-commit &&
	test_hook_disabled --pre-commit --no-pre-commit &&
	test_hook_disabled --pre-commit --no-verify &&
	test_hook_disabled --no-verify &&
	test_hook_disabled --verify --no-pre-commit &&
	test_hook_disabled --verify --no-verify &&
	test_hook_disabled --no-verify --no-commit-msg &&
	test_hook_disabled --no-verify --commit-msg
'

# a hook that checks $GIT_PREFIX and succeeds inside the
# success/ subdirectory only
cat > "$HOOK" <<EOF
#!/bin/sh
test \$GIT_PREFIX = success/
EOF

test_expect_success 'with hook requiring GIT_PREFIX' '

	echo "more content" >> file &&
	git add file &&
	mkdir success &&
	(
		cd success &&
		git commit -m "hook requires GIT_PREFIX = success/"
	) &&
	rmdir success
'

test_expect_success 'with failing hook requiring GIT_PREFIX' '

	echo "more content" >> file &&
	git add file &&
	mkdir fail &&
	(
		cd fail &&
		test_must_fail git commit -m "hook must fail"
	) &&
	rmdir fail &&
	git checkout -- file
'

test_expect_success 'check the author in hook' '
	write_script "$HOOK" <<-\EOF &&
	test "$GIT_AUTHOR_NAME" = "New Author" &&
	test "$GIT_AUTHOR_EMAIL" = "newauthor@example.com"
	EOF
	test_must_fail git commit --allow-empty -m "by a.u.thor" &&
	(
		GIT_AUTHOR_NAME="New Author" &&
		GIT_AUTHOR_EMAIL="newauthor@example.com" &&
		export GIT_AUTHOR_NAME GIT_AUTHOR_EMAIL &&
		git commit --allow-empty -m "by new.author via env" &&
		git show -s
	) &&
	git commit --author="New Author <newauthor@example.com>" \
		--allow-empty -m "by new.author via command line" &&
	git show -s
'

test_done
