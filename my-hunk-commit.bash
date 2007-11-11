#!/bin/bash

# Copyright (C) 2006 Johannes E. Schindelin
# Distributed under the same license as git.

# Use this command to commit just a few hunks of the current output
# of "git diff". For your security, it only works when the index matches
# HEAD.

# ensure that this is a git repository
. "$(git --exec-path)"/git-sh-setup

TMP_FILE=$GIT_DIR/tmp.$$.txt

trap "rm -f $TMP_FILE; exit" 0 1 2 3 15

# the index must match the HEAD
if [ -n "$(git diff --cached --name-only HEAD)" ]; then
	echo "The staging area (AKA index) is already dirty."
	exit 1
fi

# read the names of all modified files into the array "modified"

declare -a modified
filenr=1
git ls-files --modified -z > $TMP_FILE
while read -d $'\0' file; do
	modified[$filenr]="$file"
	filenr=$(($filenr+1))
done < $TMP_FILE

if [ ${#modified[*]} = 0 ]; then
	echo "No modified files."
	exit 1
fi

declare -a hunks

# interactively show the hunks of a file and ask if they should be committed.
# 1st parameter is the index into the modified file list.
# 2nd parameter should be "true" for darcs mode, empty otherwise.
#	Darcs mode means that all hunks are presented one after another.
#	Normal mode means user can specify hunks interactively.

select_hunks () {
	local index=$1
	local darcs_mode=$2
	local filename=${modified[$index]}
	local -a diff
	local -a hunk_start
	local current_hunks=${hunks[$index]}
	local lineno
	local hunkno
	local action
	local i
	local active

	lineno=1
	hunkno=0
	git diff "$filename" > $TMP_FILE
	while read line; do
		diff[$lineno]="$line"
		case "$line" in
		@@*)
			hunk_start[$hunkno]=$lineno
			hunkno=$(($hunkno+1))
			;;
		esac
		lineno=$(($lineno+1))
	done < $TMP_FILE

	hunk_start[$hunkno]=$lineno

	action=""
	while [ "$action" != commit -a "$action" != abort ]; do
		case "$darcs_mode" in
		'')
			echo
			echo "Current hunks: ($current_hunks) of $hunkno hunks"
			echo "To show (and decide on) a hunk type in the number."
			echo "To commit the current hunks, say 'commit', else 'abort'."
			echo
			echo -n "Your choice? "
			read action
			;;
		[1-9]*)
			darcs_mode=$(($darcs_mode+1))
			if [ $darcs_mode -gt $hunkno ]; then
				action=commit
			else
				action=$darcs_mode
			fi
			;;
		*)
			darcs_mode=1
			action=1
			;;
		esac
		case "$action" in
		c) action=commit;;
		q|a) action=abort;;
		commit|abort) ;;
		[1-9]*)
			echo
			for ((i=${hunk_start[$(($action-1))]}; i<${hunk_start[$action]}; i++)); do
				if [ -n "$darcs_mode" -a $i = ${hunk_start[0]} ]; then
					echo "File: $filename"
				fi
				echo ${diff[$i]}
			done | less -FSX
			active=$(echo $current_hunks,$action | tr , '\n' | sort | uniq -u | tr '\n' , | sed -e "s/^,//" -e "s/,$//")
			if [ ${#active} -lt ${#current_hunks} ]; then
				i=yes
			else
				i=no
			fi
			echo
			while [ -n "$action" -a "$action" != yes -a "$action" != no -a -n "$action" ]; do
				echo -n "Commit this hunk (default is $i)? "
				read action
				case "$action" in
				y) action=yes;;
				n) action=no;;
				esac
			done
			if [ -n "$action" -a $i != "$action" ]; then
				current_hunks=$active
			fi
			;;
		*) echo "Unknown command: $action";;
		esac
	done

	if [ "$action" = commit ]; then
		hunks[$index]=$current_hunks
	fi
}

# Apply the hunks saved in the array hunks for the specified file.
# This means that the diff is rewritten to skip the unwanted hunks.

apply_hunks () {
	local index=$1
	local filename=${modified[$index]}
	local -a current_hunks
	local lineno
	local lineno2
	local linediff
	local hunkno
	local i
	local active

	i=0
	echo ${hunks[$index]} | tr , '\n' > $TMP_FILE
	while read hunkno; do
		current_hunks[$i]=$hunkno
		i=$(($i+1))
	done < $TMP_FILE

	linediff=0
	hunkno=0
	i=0
	active=true
	git diff "$filename" > $TMP_FILE
	while read line
	do
		case "$line" in
		@@*)
			hunkno=$(($hunkno+1))
			if [ $hunkno = "${current_hunks[$i]}" ]; then
				active=true
				i=$(($i+1))
				if [ $linediff -ne 0 ]; then
					lineno=$(echo "$line" | sed "s/^.*+\([0-9]*\)[, ].*$/\1/")
					lineno2=$(($lineno+$linediff))
					line="$(echo "$line" | sed "s/+$lineno/+$lineno2/")"
				fi
			else
				active=
				lineno=$(echo "$line" | sed -n "s/^.*-[0-9]*,\([0-9]*\) .*$/\1/p")
				if [ -z "$lineno" ]; then
					lineno=1
				fi
				lineno2=$(echo "$line" | sed -n "s/^.*+[0-9]*,\([0-9]*\) .*$/\1/p")
				if [ -z "$lineno2" ]; then
					lineno2=1
				fi
				linediff=$(($linediff+$lineno-$lineno2))
			fi
			;;
		esac
		if [ -n "$active" ]; then
			echo "$line"
		fi
	done < $TMP_FILE
}

darcs_mode=
case "$1" in
--darcs) darcs_mode=true;;
esac

IFS=''
action=
i=
while [ "$action" != commit -a "$action" != abort ]; do
	case "$darcs_mode" in
	'')
		echo
		for ((i=1; i<$filenr; i++)); do
			echo -n "$i ${modified[$i]}"
			if [ -n "${hunks[$i]}" ]; then
				echo " (${hunks[$i]})"
			else
				echo
			fi
		done | less -FSX
		echo
		echo "To put one or more hunks of a file into the staging area (AKA"
		echo "index), type in the number of the file."
		echo "To commit, say 'commit', to abort, say 'abort'."
		echo
		echo -n "Your choice? "
		read action
		;;
	true)
		if [ -z "$i" ]; then
			i=1
		else
			i=$(($i+1))
		fi
		if [ $i -ge $filenr ]; then
			action=commit
		else
			action=$i
		fi
		;;
	esac
	case "$action" in
	c) action=commit;;
	q|a) action=abort;;
	commit|abort) ;;
	[0-9]*) select_hunks "$action" "$darcs_mode";;
	*) echo "Unknown command." ;;
	esac
done

if [ "$action" = commit ]; then
	for ((i=1; i<$filenr; i++)); do
		if [ -n "${hunks[$i]}" ]; then
			apply_hunks $i
		fi
	done | tee a123 | git apply --cached || exit
	git commit
fi

