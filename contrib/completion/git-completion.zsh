#compdef git gitk

# zsh completion wrapper for git
#
# Copyright (c) 2012-2013 Felipe Contreras <felipe.contreras@gmail.com>
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

zstyle -s ":completion:*:*:git:*" script script
test -z "$script" && script="$(dirname ${funcsourcetrace[1]%:*})"/git-completion.bash
ZSH_VERSION='' . "$script"

__gitcomp ()
{
	emulate -L zsh

	local cur_="${3-$cur}"

	case "$cur_" in
	--*=)
		;;
	*)
		local c IFS=$' \t\n'
		local -a array
		for c in ${=1}; do
			c="$c${4-}"
			case $c in
			--*=*|*.) ;;
			*) c="$c " ;;
			esac
			array+=("$c")
		done
		compset -P '*[=:]'
		compadd -Q -S '' -p "${2-}" -a -- array && _ret=0
		;;
	esac
}

__gitcomp_nl ()
{
	emulate -L zsh

	local IFS=$'\n'
	compset -P '*[=:]'
	compadd -Q -S "${4- }" -p "${2-}" -- ${=1} && _ret=0
}

__gitcomp_file ()
{
	emulate -L zsh

	local IFS=$'\n'
	compset -P '*[=:]'
	compadd -Q -p "${2-}" -f -- ${=1} && _ret=0
}

__git_zsh_bash_func ()
{
	emulate -L ksh

	local command=$1

	local completion_func="_git_${command//-/_}"
	declare -f $completion_func >/dev/null && $completion_func && return

	local expansion=$(__git_aliased_command "$command")
	if [ -n "$expansion" ]; then
		completion_func="_git_${expansion//-/_}"
		declare -f $completion_func >/dev/null && $completion_func
	fi
}

__git_zsh_main ()
{
	local curcontext="$curcontext" state state_descr line
	typeset -A opt_args
	local -a orig_words

	orig_words=( ${words[@]} )

	_arguments -C \
		'(-p --paginate --no-pager)'{-p,--paginate}'[Pipe all output into ''less'']' \
		'(-p --paginate)--no-pager[Do not pipe git output into a pager]' \
		'--git-dir=-[Set the path to the repository]: :_directories' \
		'--bare[Treat the repository as a bare repository]' \
		'(- :)--version[Prints the git suite version]' \
		'--exec-path=-[Path to where your core git programs are installed]:: :_directories' \
		'--html-path[Print the path where git''s HTML documentation is installed]' \
		'--info-path[Print the path where the Info files are installed]' \
		'--man-path[Print the manpath (see `man(1)`) for the man pages]' \
		'--work-tree=-[Set the path to the working tree]: :_directories' \
		'--namespace=-[Set the git namespace]' \
		'--no-replace-objects[Do not use replacement refs to replace git objects]' \
		'(- :)--help[Prints the synopsis and a list of the most commonly used commands]: :->arg' \
		'(-): :->command' \
		'(-)*:: :->arg' && return

	case $state in
	(command)
		emulate ksh -c __git_compute_porcelain_commands
		local -a porcelain aliases
		porcelain=( ${=__git_porcelain_commands} )
		aliases=( $(emulate ksh -c __git_aliases) )
		_describe -t porcelain-commands 'porcelain commands' porcelain && _ret=0
		_describe -t aliases 'aliases' aliases && _ret=0
		;;
	(arg)
		local command="${words[1]}" __git_dir

		if (( $+opt_args[--bare] )); then
			__git_dir='.'
		else
			__git_dir=${opt_args[--git-dir]}
		fi

		(( $+opt_args[--help] )) && command='help'

		words=( ${orig_words[@]} )

		__git_zsh_bash_func $command
		;;
	esac
}

_git ()
{
	local _ret=1
	local cur cword prev

	cur=${words[CURRENT]}
	prev=${words[CURRENT-1]}
	let cword=CURRENT-1

	if (( $+functions[__${service}_zsh_main] )); then
		__${service}_zsh_main
	else
		emulate ksh -c __${service}_main
	fi

	let _ret && _default -S '' && _ret=0
	return _ret
}

_git
