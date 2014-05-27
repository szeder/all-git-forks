# Shell library sourced instead of ./test-lib.sh by tests that need to
# run under Zsh; primarily intended for tests of the git-prompt.sh
# script.

if test -n "$ZSH_VERSION" && test -z "$POSIXLY_CORRECT"; then
	true
elif command -v zsh >/dev/null 2>&1; then
	unset POSIXLY_CORRECT
	exec zsh "$0" "$@"
else
	echo '1..0 #SKIP skipping Zsh-specific tests; zsh not available'
	exit 0
fi

# ensure that we are in full-on Zsh mode
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

emulate sh -c '. ./test-lib.sh'
