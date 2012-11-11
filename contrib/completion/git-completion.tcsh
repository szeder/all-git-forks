#!tcsh
#
# tcsh completion support for core Git.
#
# Copyright (C) 2012 Marc Khouzam <marc.khouzam@gmail.com>
# Distributed under the GNU General Public License, version 2.0.
#
# This script makes use of the git-completion.bash script to
# determine the proper completion for git commands under tcsh.
#
# To use this completion script:
#
#    1) Copy both this file and the bash completion script to your ${HOME} directory
#       using the names ${HOME}/.git-completion.tcsh and ${HOME}/.git-completion.bash.
#    2) Add the following line to your .tcshrc/.cshrc:
#        source ${HOME}/.git-completion.tcsh

# One can change the below line to use a different location
set __git_tcsh_completion_script = ${HOME}/.git-completion.bash

# Check that the user put the script in the right place
if ( ! -e ${__git_tcsh_completion_script} ) then
	echo "ERROR in git-completion.tcsh script.  Cannot find: ${__git_tcsh_completion_script}.  Git completion will not work."
	exit
endif

# Make the script executable if it is not
if ( ! -x ${__git_tcsh_completion_script} ) then
	chmod u+x ${__git_tcsh_completion_script}
endif

complete git  'p/*/`${__git_tcsh_completion_script} "${COMMAND_LINE}" | sort | uniq`/'
complete gitk 'p/*/`${__git_tcsh_completion_script} "${COMMAND_LINE}" | sort | uniq`/'

