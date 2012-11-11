#!tcsh
#
# tcsh completion support for core Git.
#
# Copyright (C) 2012 Ericsson
# Distributed under the GNU General Public License, version 2.0.
#
# This scripts converts the git-completion.bash script provided
# by core Git into an executable script that can be used by tcsh.
# The current script then setups the tcsh complete command to 
# use that script.
#
# To use this completion script:
#
#    1) Copy both this file and the bash completion script to the SAME directory somewhere
#       (e.g. ~/.git-completion.tcsh and ~/.git-completion.bash).
#    2) Add the following line to your .tcshrc/.cshrc:
#        source ~/.git-completion.tcsh

set __git_tcsh_completion_original_script = ${HOME}/.git-completion.bash
set __git_tcsh_completion_script = ${HOME}/.git-completion.bash.tcsh

cat << EOF > ${__git_tcsh_completion_script}
#!/bin/bash
#
# Below is the textual inclusion of the bash completion provided by git
#
########################################################################
# BEGINNING of git bash completion script
########################################################################
EOF

cat ${__git_tcsh_completion_original_script} >> ${__git_tcsh_completion_script}

cat << EOF >> ${__git_tcsh_completion_script}

########################################################################
# END of git bash completion script
########################################################################

# Method that will output the result of the completion done by
# the bash completion script, so that it can be re-used by tcsh.
_git_tcsh ()
{
	# Set COMP_WORDS and COMP_CWORD as bash would because that is what
	# the above git bash completion script expects.

	# Set COMP_WORDS to the command-line as bash would.
	COMP_WORDS=(\$1)

	# Set COMP_CWORD to the cursor location as bash would.
	# From tcsh, the provided command-line is only until the cursor,
	# so the cursor location is always at the last element.
	# We must check for a space as the last character which will
	# tell us that the previous word is complete and the cursor
	# is on the next word.
	if [ "\${1: -1}" == " " ]
	then 
		# The last character is a space, so our location is at the end
		# of the command-line array
		COMP_CWORD=\${#COMP_WORDS[@]}
	else 
		# The last character is not a space, so our location is on the
		# last word of the command-line array, so we must decrement the
		# count by 1
		COMP_CWORD=\$((\${#COMP_WORDS[@]}-1))
	fi   

	# Call _git() or _gitk() of the bash script, based on the first
	# element of the command-line
	_\${COMP_WORDS[0]}

	# Print the result that is stored in the bash variable \${COMPREPLY}
	for i in \${COMPREPLY[@]}; do
		echo "\$i" 
	done 
}

# Go ahead and run the tcsh function
_git_tcsh "\$1"

EOF

# Make the script executable
chmod u+x ${__git_tcsh_completion_script}

complete git  'p,*,`${__git_tcsh_completion_script} "${COMMAND_LINE}"`,' 
complete gitk 'p,*,`${__git_tcsh_completion_script} "${COMMAND_LINE}"`,' 

