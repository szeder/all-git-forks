#compdef git gitk

# zsh completion wrapper for git
#
# You need git's bash completion script installed somewhere, by default on the
# same directory as this script.
#
# If your script is on ~/.git-completion.sh instead, you can configure it on
# your ~/.zshrc:
#
#  zstyle ':completion:*:*:git:*' script ~/.git-completion.sh
#
# The recommended way to install this script is to copy to
# '~/.zsh/completion/_git', and then add the following to your ~/.zshrc file:
#
#  fpath=(~/.zsh/completion $fpath)

complete ()
{
	# do nothing
	return 0
}

zstyle -s ":completion:*:*:git:*" script script
test -z "$script" && script="$(dirname ${funcsourcetrace[1]%:*})"/git-completion.bash
ZSH_VERSION='' . "$script"

__gitcompadd ()
{
	compadd -Q -S "$4" -P "${(M)cur#*[=:]}" -p "$2" -- ${=1} && _ret=0
}

_git ()
{
	local _ret=1
	() {
		emulate -L ksh
		local cur cword prev
		cur=${words[CURRENT-1]}
		prev=${words[CURRENT-2]}
		let cword=CURRENT-1
		__${service}_main
	}
	let _ret && _default -S '' && _ret=0
	return _ret
}

_git
