# Shell library sourced instead of ./test-lib.sh by tests that need to
# run under Zsh; primarily intended for tests of the git-prompt.sh
# script.

if test -n "$ZSH_VERSION" && test -z "$POSIXLY_CORRECT" && [[ ! -o FUNCTION_ARGZERO ]]; then
	true
elif command -v zsh >/dev/null 2>&1; then
	unset POSIXLY_CORRECT
	# Run Zsh with the FUNCTION_ARGZERO option disabled so that
	# test-lib.sh sees the test script pathname when it examines
	# $0 instead of "./lib-zsh.sh".  (This works around a Zsh
	# limitation: 'emulate sh -c' does not restore $0 to the value
	# specified by POSIX.)
	exec zsh +o FUNCTION_ARGZERO "$0" "$@"
else
	echo '1..0 #SKIP skipping Zsh-specific tests; zsh not available'
	exit 0
fi

# ensure that we are in full-on Zsh mode.  note: this re-enables the
# FUNCTION_ARGZERO option
emulate -R zsh || exit 1

shellname=Zsh

ps1_expansion_enable () { setopt PROMPT_SUBST; }
ps1_expansion_disable () { unsetopt PROMPT_SUBST; }
set_ps1_format_vars () {
	percent='%%%%'
	c_red='%%F{red}'
	c_green='%%F{green}'
	c_lblue='%%F{blue}'
	c_clear='%%f'
}

# Due to language incompatibilities between POSIX sh and Zsh,
# test-lib.sh must be sourced in sh emulation mode.
#
# Note: Although the FUNCTION_ARGZERO option is currently enabled, sh
# emulation mode temporarily turns it off ($0 is left alone when
# sourcing test-lib.sh)
emulate -R sh -c '. ./test-lib.sh'

# Ensure that the test code is run in Zsh mode.  Because test_eval_()
# was defined by test-lib.sh inside the above 'emulate sh -c', the Zsh
# shell options that implement sh emulation will be temporarily
# toggled when test_eval_() executes.  Normally this would cause the
# test code to run in sh emulation mode, not Zsh mode.  By defining
# test_eval_override() in zsh emulation mode, the options are
# temporarily toggled back to the Zsh defaults when evaluating the
# test code.
emulate -R zsh -c 'test_eval_override () { eval "$*"; }'
