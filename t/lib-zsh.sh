# Shell library sourced instead of ./test-lib.sh by tests that need to
# run under Zsh; primarily intended for tests of the git-prompt.sh
# script.

if test -n "$ZSH_VERSION" && test -z "$POSIXLY_CORRECT" && [[ ! -o FUNCTION_ARGZERO ]]; then
	true
elif command -v zsh >/dev/null 2>&1; then
	unset POSIXLY_CORRECT
	# Run Zsh with the FUNCTION_ARGZERO option disabled so that
	# test-lib.sh sees the test script pathname when it examines
	# $0 instead of "./lib-zsh.sh".  (This works around a Zsh bug;
	# 'emulate sh -c' should temporarily restore $0 to the POSIX
	# specification for $0, but it doesn't.)
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

# note: although the FUNCTION_ARGZERO option is currently enabled, sh
# emulation mode temporarily turns it off ($0 is left alone when
# sourcing test-lib.sh)
emulate sh -c '. ./test-lib.sh'
