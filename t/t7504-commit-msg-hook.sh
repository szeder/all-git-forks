#!/bin/sh

test_description='commit-msg hook'

. ./test-lib.sh

test_expect_success 'with no hook' '

	echo "foo" > file &&
	git add file &&
	git commit -m "first"

'

# set up fake editor for interactive editing
cat > fake-editor <<'EOF'
#!/bin/sh
cp FAKE_MSG "$1"
exit 0
EOF
chmod +x fake-editor

## Not using test_set_editor here so we can easily ensure the editor variable
## is only set for the editor tests
FAKE_EDITOR="$(pwd)/fake-editor"
export FAKE_EDITOR

test_expect_success 'with no hook (editor)' '

	echo "more foo" >> file &&
	git add file &&
	echo "more foo" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit

'

test_expect_success '--no-commit-msg with no hook' '

	echo "bar" > file &&
	git add file &&
	git commit --no-commit-msg -m "bar"

'

test_expect_success '--no-commit-msg with no hook (editor)' '

	echo "more bar" > file &&
	git add file &&
	echo "more bar" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit --no-commit-msg

'

# now install hook that always succeeds
HOOKDIR="$(git rev-parse --git-dir)/hooks"
HOOK="$HOOKDIR/commit-msg"
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

test_expect_success 'with succeeding hook (editor)' '

	echo "more more" >> file &&
	git add file &&
	echo "more more" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit

'

test_expect_success '--no-commit-msg with succeeding hook' '

	echo "even more" >> file &&
	git add file &&
	git commit --no-commit-msg -m "even more"

'

test_expect_success '--no-commit-msg with succeeding hook (editor)' '

	echo "even more more" >> file &&
	git add file &&
	echo "even more more" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit --no-commit-msg

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

test_expect_success 'with failing hook (editor)' '

	echo "more another" >> file &&
	git add file &&
	echo "more another" > FAKE_MSG &&
	! (GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit)

'

test_expect_success '--no-commit-msg with failing hook' '

	echo "stuff" >> file &&
	git add file &&
	git commit --no-commit-msg -m "stuff"

'

test_expect_success '--no-commit-msg with failing hook (editor)' '

	echo "more stuff" >> file &&
	git add file &&
	echo "more stuff" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit --no-commit-msg

'

chmod -x "$HOOK"
test_expect_success POSIXPERM 'with non-executable hook' '

	echo "content" >> file &&
	git add file &&
	git commit -m "content"

'

test_expect_success POSIXPERM 'with non-executable hook (editor)' '

	echo "content again" >> file &&
	git add file &&
	echo "content again" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit -m "content again"

'

test_expect_success POSIXPERM '--no-commit-msg with non-executable hook' '

	echo "more content" >> file &&
	git add file &&
	git commit --no-commit-msg -m "more content"

'

test_expect_success POSIXPERM '--no-commit-msg with non-executable hook (editor)' '

	echo "even more content" >> file &&
	git add file &&
	echo "even more content" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit --no-commit-msg

'

test_hook_enabled () {
	git checkout --detach master &&
	output="running failing commit-msg hook with ${*:-(none)}..." &&
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
	output="running failing commit-msg hook with ${*:-(none)}..." &&
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
	test_hook_enabled --commit-msg &&
	test_hook_enabled --no-commit-msg --commit-msg &&
	test_hook_enabled --no-verify --commit-msg &&
	test_hook_enabled --verify &&
	test_hook_enabled --no-commit-msg --verify &&
	test_hook_enabled --no-verify --verify &&
	test_hook_enabled --verify --no-pre-commit &&
	test_hook_enabled --verify --pre-commit &&
	test_hook_disabled --no-commit-msg &&
	test_hook_disabled --commit-msg --no-commit-msg &&
	test_hook_disabled --commit-msg --no-verify &&
	test_hook_disabled --no-verify &&
	test_hook_disabled --verify --no-commit-msg &&
	test_hook_disabled --verify --no-verify &&
	test_hook_disabled --no-verify --no-pre-commit &&
	test_hook_disabled --no-verify --pre-commit
'

# now a hook that edits the commit message
cat > "$HOOK" <<'EOF'
#!/bin/sh
echo "new message" > "$1"
exit 0
EOF
chmod +x "$HOOK"

commit_msg_is () {
	test "`git log --pretty=format:%s%b -1`" = "$1"
}

test_expect_success 'hook edits commit message' '

	echo "additional" >> file &&
	git add file &&
	git commit -m "additional" &&
	commit_msg_is "new message"

'

test_expect_success 'hook edits commit message (editor)' '

	echo "additional content" >> file &&
	git add file &&
	echo "additional content" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit &&
	commit_msg_is "new message"

'

test_expect_success "hook doesn't edit commit message" '

	echo "plus" >> file &&
	git add file &&
	git commit --no-commit-msg -m "plus" &&
	commit_msg_is "plus"

'

test_expect_success "hook doesn't edit commit message (editor)" '

	echo "more plus" >> file &&
	git add file &&
	echo "more plus" > FAKE_MSG &&
	GIT_EDITOR="\"\$FAKE_EDITOR\"" git commit --no-commit-msg &&
	commit_msg_is "more plus"

'

test_done
