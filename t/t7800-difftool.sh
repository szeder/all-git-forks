#!/bin/sh
#
# Copyright (c) 2009, 2010, 2012 David Aguilar
#

test_description='git-difftool

Testing basic diff tool invocation
'

. ./test-lib.sh

difftool_test_setup()
{
	test_config diff.tool test-tool &&
	test_config difftool.test-tool.cmd 'cat $LOCAL' &&
	test_config difftool.bogus-tool.cmd false
}

prompt_given()
{
	prompt="$1"
	test "$prompt" = "Launch 'test-tool' [Y/n]: branch"
}

stdin_contains()
{
	grep >/dev/null "$1"
}

stdin_doesnot_contain()
{
	! stdin_contains "$1"
}

# Create a file on master and change it on branch
test_expect_success PERL 'setup' '
	echo master >file &&
	git add file &&
	git commit -m "added file" &&

	git checkout -b branch master &&
	echo branch >file &&
	git commit -a -m "branch changed file" &&
	git checkout master
'

# Configure a custom difftool.<tool>.cmd and use it
test_expect_success PERL 'custom commands' '
	difftool_test_setup &&
	test_config difftool.test-tool.cmd "cat \$REMOTE" &&
	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "master" &&

	test_config difftool.test-tool.cmd "cat \$LOCAL" &&
	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "branch"
'

# Ensures that a custom difftool.<tool>.cmd overrides built-ins
test_expect_success PERL 'custom commands override built-ins' '
	test_config difftool.defaults.cmd "cat \$REMOTE" &&

	diff=$(git difftool --tool defaults --no-prompt branch) &&
	test "$diff" = "master"
'

# Ensures that git-difftool ignores bogus --tool values
test_expect_success PERL 'difftool ignores bad --tool values' '
	diff=$(git difftool --no-prompt --tool=bad-tool branch)
	test "$?" = 1 &&
	test -z "$diff"
'

test_expect_success PERL 'difftool forwards arguments to diff' '
	difftool_test_setup &&
	>for-diff &&
	git add for-diff &&
	echo changes>for-diff &&
	git add for-diff &&
	diff=$(git difftool --cached --no-prompt -- for-diff) &&
	test "$diff" = "" &&
	git reset -- for-diff &&
	rm for-diff
'

test_expect_success PERL 'difftool honors --gui' '
	difftool_test_setup &&
	test_config merge.tool bogus-tool &&
	test_config diff.tool bogus-tool &&
	test_config diff.guitool test-tool &&

	diff=$(git difftool --no-prompt --gui branch) &&
	test "$diff" = "branch"
'

test_expect_success PERL 'difftool --gui last setting wins' '
	difftool_test_setup &&

	diff=$(git difftool --no-prompt --gui --no-gui) &&
	test -z "$diff" &&

	test_config merge.tool bogus-tool &&
	test_config diff.tool bogus-tool &&
	test_config diff.guitool test-tool &&

	diff=$(git difftool --no-prompt --no-gui --gui branch) &&
	test "$diff" = "branch"
'

test_expect_success PERL 'difftool --gui works without configured diff.guitool' '
	difftool_test_setup &&

	diff=$(git difftool --no-prompt --gui branch) &&
	test "$diff" = "branch"
'

# Specify the diff tool using $GIT_DIFF_TOOL
test_expect_success PERL 'GIT_DIFF_TOOL variable' '
	difftool_test_setup &&
	git config --unset diff.tool &&

	GIT_DIFF_TOOL=test-tool &&
	export GIT_DIFF_TOOL &&

	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "branch" &&
	sane_unset GIT_DIFF_TOOL
'

# Test the $GIT_*_TOOL variables and ensure
# that $GIT_DIFF_TOOL always wins unless --tool is specified
test_expect_success PERL 'GIT_DIFF_TOOL overrides' '
	difftool_test_setup &&
	test_config diff.tool bogus-tool &&
	test_config merge.tool bogus-tool &&
	GIT_DIFF_TOOL=test-tool &&
	export GIT_DIFF_TOOL &&

	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "branch" &&

	test_config diff.tool bogus-tool &&
	test_config merge.tool bogus-tool &&
	GIT_DIFF_TOOL=bogus-tool &&
	export GIT_DIFF_TOOL &&

	diff=$(git difftool --no-prompt --tool=test-tool branch) &&
	test "$diff" = "branch" &&
	sane_unset GIT_DIFF_TOOL
'

# Test that we don't have to pass --no-prompt to difftool
# when $GIT_DIFFTOOL_NO_PROMPT is true
test_expect_success PERL 'GIT_DIFFTOOL_NO_PROMPT variable' '
	difftool_test_setup &&
	GIT_DIFFTOOL_NO_PROMPT=true &&
	export GIT_DIFFTOOL_NO_PROMPT &&

	diff=$(git difftool branch) &&
	test "$diff" = "branch" &&
	sane_unset GIT_DIFFTOOL_NO_PROMPT
'

# git-difftool supports the difftool.prompt variable.
# Test that GIT_DIFFTOOL_PROMPT can override difftool.prompt = false
test_expect_success PERL 'GIT_DIFFTOOL_PROMPT variable' '
	difftool_test_setup &&
	test_config difftool.prompt false &&
	GIT_DIFFTOOL_PROMPT=true &&
	export GIT_DIFFTOOL_PROMPT &&

	prompt=$(echo | git difftool branch | tail -1) &&
	prompt_given "$prompt" &&
	sane_unset GIT_DIFFTOOL_PROMPT
'

# Test that we don't have to pass --no-prompt when difftool.prompt is false
test_expect_success PERL 'difftool.prompt config variable is false' '
	difftool_test_setup &&
	test_config difftool.prompt false &&

	diff=$(git difftool branch) &&
	test "$diff" = "branch"
'

# Test that we don't have to pass --no-prompt when mergetool.prompt is false
test_expect_success PERL 'difftool merge.prompt = false' '
	difftool_test_setup &&
	test_might_fail git config --unset difftool.prompt &&
	test_config mergetool.prompt false &&

	diff=$(git difftool branch) &&
	test "$diff" = "branch"
'

# Test that the -y flag can override difftool.prompt = true
test_expect_success PERL 'difftool.prompt can overridden with -y' '
	difftool_test_setup &&
	test_config difftool.prompt true &&

	diff=$(git difftool -y branch) &&
	test "$diff" = "branch"
'

# Test that the --prompt flag can override difftool.prompt = false
test_expect_success PERL 'difftool.prompt can overridden with --prompt' '
	difftool_test_setup &&
	test_config difftool.prompt false &&

	prompt=$(echo | git difftool --prompt branch | tail -1) &&
	prompt_given "$prompt"
'

# Test that the last flag passed on the command-line wins
test_expect_success PERL 'difftool last flag wins' '
	difftool_test_setup &&
	diff=$(git difftool --prompt --no-prompt branch) &&
	test "$diff" = "branch" &&

	prompt=$(echo | git difftool --no-prompt --prompt branch | tail -1) &&
	prompt_given "$prompt"
'

# git-difftool falls back to git-mergetool config variables
# so test that behavior here
test_expect_success PERL 'difftool + mergetool config variables' '
	test_config merge.tool test-tool &&
	test_config mergetool.test-tool.cmd "cat \$LOCAL" &&

	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "branch" &&

	# set merge.tool to something bogus, diff.tool to test-tool
	test_config merge.tool bogus-tool &&
	test_config diff.tool test-tool &&

	diff=$(git difftool --no-prompt branch) &&
	test "$diff" = "branch"
'

test_expect_success PERL 'difftool.<tool>.path' '
	test_config difftool.tkdiff.path echo &&
	diff=$(git difftool --tool=tkdiff --no-prompt branch) &&
	lines=$(echo "$diff" | grep file | wc -l) &&
	test "$lines" -eq 1
'

test_expect_success PERL 'difftool --extcmd=cat' '
	diff=$(git difftool --no-prompt --extcmd=cat branch) &&
	test "$diff" = branch"$LF"master
'

test_expect_success PERL 'difftool --extcmd cat' '
	diff=$(git difftool --no-prompt --extcmd cat branch) &&
	test "$diff" = branch"$LF"master
'

test_expect_success PERL 'difftool -x cat' '
	diff=$(git difftool --no-prompt -x cat branch) &&
	test "$diff" = branch"$LF"master
'

test_expect_success PERL 'difftool --extcmd echo arg1' '
	diff=$(git difftool --no-prompt --extcmd sh\ -c\ \"echo\ \$1\" branch) &&
	test "$diff" = file
'

test_expect_success PERL 'difftool --extcmd cat arg1' '
	diff=$(git difftool --no-prompt --extcmd sh\ -c\ \"cat\ \$1\" branch) &&
	test "$diff" = master
'

test_expect_success PERL 'difftool --extcmd cat arg2' '
	diff=$(git difftool --no-prompt --extcmd sh\ -c\ \"cat\ \$2\" branch) &&
	test "$diff" = branch
'

# Create a second file on master and a different version on branch
test_expect_success PERL 'setup with 2 files different' '
	echo m2 >file2 &&
	git add file2 &&
	git commit -m "added file2" &&

	git checkout branch &&
	echo br2 >file2 &&
	git add file2 &&
	git commit -a -m "branch changed file2" &&
	git checkout master
'

test_expect_success PERL 'say no to the first file' '
	diff=$( (echo n; echo) | git difftool -x cat branch ) &&

	echo "$diff" | stdin_contains m2 &&
	echo "$diff" | stdin_contains br2 &&
	echo "$diff" | stdin_doesnot_contain master &&
	echo "$diff" | stdin_doesnot_contain branch
'

test_expect_success PERL 'say no to the second file' '
	diff=$( (echo; echo n) | git difftool -x cat branch ) &&

	echo "$diff" | stdin_contains master &&
	echo "$diff" | stdin_contains branch &&
	echo "$diff" | stdin_doesnot_contain m2 &&
	echo "$diff" | stdin_doesnot_contain br2
'

test_expect_success PERL 'difftool --tool-help' '
	tool_help=$(git difftool --tool-help) &&
	echo "$tool_help" | stdin_contains tool
'

test_expect_success PERL 'setup change in subdirectory' '
	git checkout master &&
	mkdir sub &&
	echo master >sub/sub &&
	git add sub/sub &&
	git commit -m "added sub/sub" &&
	echo test >>file &&
	echo test >>sub/sub &&
	git add . &&
	git commit -m "modified both"
'

test_expect_success PERL 'difftool -d' '
	diff=$(git difftool -d --extcmd ls branch) &&
	echo "$diff" | stdin_contains sub &&
	echo "$diff" | stdin_contains file
'

test_expect_success PERL 'difftool --dir-diff' '
	diff=$(git difftool --dir-diff --extcmd ls branch) &&
	echo "$diff" | stdin_contains sub &&
	echo "$diff" | stdin_contains file
'

test_expect_success PERL 'difftool --dir-diff ignores --prompt' '
	diff=$(git difftool --dir-diff --prompt --extcmd ls branch) &&
	echo "$diff" | stdin_contains sub &&
	echo "$diff" | stdin_contains file
'

test_expect_success PERL 'difftool --dir-diff from subdirectory' '
	(
		cd sub &&
		diff=$(git difftool --dir-diff --extcmd ls branch) &&
		echo "$diff" | stdin_contains sub &&
		echo "$diff" | stdin_contains file
	)
'

test_done
