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

zstyle -T ':completion:*:*:git:*' tag-order && \
	zstyle ':completion:*:*:git:*' tag-order 'common-commands porcelain-commands'

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

__git_zsh_cmd_common ()
{
	local -a list
	list=(
	add:'add file contents to the index'
	bisect:'find by binary search the change that introduced a bug'
	branch:'list, create, or delete branches'
	checkout:'checkout a branch or paths to the working tree'
	clone:'clone a repository into a new directory'
	commit:'record changes to the repository'
	diff:'show changes between commits, commit and working tree, etc'
	fetch:'download objects and refs from another repository'
	grep:'print lines matching a pattern'
	init:'create an empty Git repository or reinitialize an existing one'
	log:'show commit logs'
	merge:'join two or more development histories together'
	mv:'move or rename a file, a directory, or a symlink'
	pull:'fetch from and merge with another repository or a local branch'
	push:'update remote refs along with associated objects'
	rebase:'forward-port local commits to the updated upstream head'
	reset:'reset current HEAD to the specified state'
	rm:'remove files from the working tree and from the index'
	show:'show various types of objects'
	status:'show the working tree status'
	tag:'create, list, delete or verify a tag object signed with GPG')
	_describe -t common-commands 'common commands' list && _ret=0
}

__git_zsh_cmd_porcelain ()
{
	local -a list
	list=(
	am:'apply a series of patches from a mailbox'
	archive:'create an archive of files from a named tree'
	bundle:'move objects and refs by archive'
	cherry-pick:'apply the changes introduced by some existing commits'
	citool:'graphical alternative to `git commit`'
	clean:'remove untracked files from the working tree'
	describe:'show the most recent tag that is reachable from a commit'
	format-patch:'prepare patches for e-mail submission'
	gc:'cleanup unnecessary files and optimize the local repository'
	gui:'a portable graphical interface to Git'
	notes:'add or inspect object notes'
	revert:'revert some existing commits'
	shortlog:'summarize `git log` output'
	stash:'stash the changes in a dirty working directory away'
	submodule:'initialize, update or inspect submodules')
	_describe -t porcelain-commands 'porcelain commands' list && _ret=0
}

__git_zsh_cmd_ancillary ()
{
	local -a list
	list=(
	annotate:'annotate file lines with commit information'
	blame:'show what revision and author last modified each line of a file'
	cherry:'find commits not merged upstream'
	count-objects:'count unpacked number of objects and their disk consumption'
	difftool:'show changes using common diff tools'
	fsck:'verifies the connectivity and validity of the objects in the database'
	get-tar-commit-id:'extract commit ID from an archive created using git-archive'
	help:'display help information about Git'
	instaweb:'instantly browse your working repository in gitweb'
	merge-tree:'show three-way merge without touching index'
	rerere:'reuse recorded resolution of conflicted merges'
	rev-parse:'pick out and massage parameters'
	show-branch:'show branches and their commits'
	verify-tag:'check the GPG signature of tags'
	whatchanged:'show logs with difference each commit introduces'
	config:'get and set repository or global options'
	fast-export:'git data exporter'
	fast-import:'backend for fast Git data importers'
	filter-branch:'rewrite branches'
	lost-found:'recover lost refs that luckily have not yet been pruned'
	mergetool:'run merge conflict resolution tools to resolve merge conflicts'
	pack-refs:'pack heads and tags for efficient repository access'
	prune:'prune all unreachable objects from the object database'
	reflog:'manage reflog information'
	relink:'hardlink common objects in local repositories'
	remote:'manage set of tracked repositories'
	repack:'pack unpacked objects in a repository'
	replace:'create, list, delete refs to replace objects'
	repo-config:'get and set repository or global options o')
	_describe -t ancillary-commands 'ancillary commands' list && _ret=0
}

__git_zsh_cmd_plumbing ()
{
	local -a list
	list=(
	cat-file:'provide content or type and size information for repository objects'
	diff-files:'compares files in the working tree and the index'
	diff-index:'compares content and mode of blobs between the index and repository'
	diff-tree:'compares the content and mode of blobs found via two tree objects'
	for-each-ref:'output information on each ref'
	ls-files:'show information about files in the index and the working tree'
	ls-remote:'list references in a remote repository'
	ls-tree:'list the contents of a tree object'
	merge-base:'find as good common ancestors as possible for a merge'
	name-rev:'find symbolic names for given revs'
	pack-redundant:'find redundant pack files'
	rev-list:'lists commit objects in reverse chronological order'
	show-index:'show packed archive index'
	show-ref:'list references in a local repository'
	tar-tree:'create a tar archive of the files in the named tree object'
	unpack-file:'creates a temporary file with a blob.s contents'
	var:'show a Git logical variable'
	verify-pack:'validate packed Git archive files'
	apply:'apply a patch to files and/or to the index'
	checkout-index:'copy files from the index to the working tree'
	commit-tree:'create a new commit object'
	hash-object:'compute object ID and optionally creates a blob from a file'
	index-pack:'build pack index file for an existing packed archive'
	merge-file:'run a three-way file merge'
	merge-index:'run a merge for files needing merging'
	mktag:'creates a tag object'
	mktree:'build a tree-object from ls-tree formatted text'
	pack-objects:'create a packed archive of objects'
	prune-packed:'remove extra objects that are already in pack files'
	read-tree:'reads tree information into the index'
	symbolic-ref:'read, modify and delete symbolic refs'
	unpack-objects:'unpack objects from a packed archive'
	update-index:'register file contents in the working tree to the index'
	update-ref:'update the object name stored in a ref safely'
	write-tree:'create a tree object from the current index')
	_describe -t plumbing-commands 'plumbing commands' list && _ret=0
}

__git_zsh_cmd_helper ()
{
	local -a list
	list=(
	daemon:'a really simple server for Git repositories'
	fetch-pack:'receive missing objects from another repository'
	http-backend:'server side implementation of Git over HTTP'
	send-pack:'push objects over Git protocol to another repository'
	update-server-info:'update auxiliary info file to help dumb servers'
	check-attr:'display gitattributes information'
	check-ignore:'debug gitignore / exclude files'
	check-ref-format:'ensures that a reference name is well formed'
	column:'display data in columns'
	credential:'retrieve and store user credentials'
	credential-cache:'helper to temporarily store passwords in memory'
	credential-store:'helper to store credentials on disk'
	fmt-merge-msg:'produce a merge commit message'
	mailinfo:'extracts patch and authorship from a single e-mail message'
	mailsplit:'simple UNIX mbox splitter program'
	merge-one-file:'the standard helper program to use with git-merge-index'
	patch-id:'compute unique ID for a patch'
	peek-remote:'list the references in a remote repository'
	sh-i18n:'git.s i18n setup code for shell scripts'
	sh-setup:'common Git shell script setup code'
	stripspace:'remove unnecessary whitespace'
	http-fetch:'download from a remote Git repository via HTTP'
	http-push:'push objects over HTTP/DAV to another repository'
	parse-remote:'routines to help parsing remote repository access parameters'
	receive-pack:'receive what is pushed into the repository'
	shell:'restricted login shell for Git-only SSH access'
	upload-archive:'send archive back to git-archive'
	upload-pack:'send objects packed back to git-fetch-pack')
	_describe -t helper-commands 'helper commands' list && _ret=0
}

__git_zsh_cmd_foreign ()
{
	local -a list
	list=(
	archimport:'import an Arch repository into Git'
	cvsexportcommit:'export a single commit to a CVS checkout'
	cvsimport:'salvage your data out of another SCM people love to hate'
	cvsserver:'a CVS server emulator for Git'
	imap-send:'send a collection of patches from stdin to an IMAP folder'
	p4:'import from and submit to Perforce repositories'
	quiltimport:'applies a quilt patchset onto the current branch'
	request-pull:'generates a summary of pending changes'
	send-email:'send a collection of patches as emails'
	svn:'bidirectional operation between a Subversion repository and Git')
	_describe -t foreign-commands 'foreign commands' list && _ret=0
}

__git_zsh_cmd_alias ()
{
	local -a list
	list=(${^${${(0)"$(_call_program aliases "git config -z --get-regexp '^alias.'")"}#alias.}/$'\n'/:alias for \'}\')
	_describe -t alias-commands 'aliases' list $*
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
		_alternative \
                         'alias-commands:aliases:__git_zsh_cmd_alias' \
                         'common-commands:common:__git_zsh_cmd_common' \
                         'porcelain-commands:porcelain:__git_zsh_cmd_porcelain' \
                         'ancillary-commands:ancillary:__git_zsh_cmd_ancillary' \
                         'plumbing-commands:plumbing:__git_zsh_cmd_plumbing' \
                         'helper-commands:helper:__git_zsh_cmd_helper' \
                         'foreign-commands:foreign:__git_zsh_cmd_foreign' && _ret=0
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
