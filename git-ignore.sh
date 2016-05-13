#!/bin/sh
#
# Copyright (c) 2016, Thurston Stone

DEBUG=0

SUBDIRECTORY_OK=Yes
OPTIONS_KEEPDASHDASH=
OPTIONS_STUCKLONG=t
# Would be nice to have examples, but rev-parse sees '*' as a symbol to hide everything afterwards
#e,ext           add relative path for any file of that type (ex. path/to/*.ext)
#E,all-ext       all files of that extention anywhere (ex. **/*.ext)
#d,dir           all files under the parent directory (ex. directory/*)
#a,all-file      all files of that file name (ex. **/filename.ext)
OPTIONS_SPEC="git ignore [options] [file|glob ...]
--
 Determine what files to add to the gitignore(s):
n,dry-run       do not actually edit any .gitignore files
e,ext           add relative path for any file of that type
E,all-ext       all files of that extention anywhere
d,dir           all files under the parent directory
a,all-file      all files of that file name
 Determine what gitignore(s) to use:
p,parent-level=  number of parent directories containing the gitignore to edit. Set to 0 to put it in the local directory"

. git-sh-setup
. git-sh-i18n

debug () {
date
    if test $DEBUG -eq 1
    then
        message=$1
        echo "$(eval_gettext "DEBUG: \$message")"
    fi
}

add_ignore () {
    # get the absolute path of the file
    file="$(cd "$(dirname "$1")"; pwd)/$(basename "$1")"
    debug "file=$file"

    directory="$(dirname "$file")/"
    debug "directory=$directory"
    repo_root=${repo_root_nix}
    rel_directory="${directory#$repo_root}"
    if test "$rel_directory" = "$directory"
    then
        # repo root 2 (cygwin-ified path) didn't work
        # try the other one
        rel_directory="${directory#$repo_root_raw}"
    fi
    debug "rel_directory=$rel_directory"
    filename=$(basename "$file")
    debug "filename=$filename"
    line="${rel_directory}${filename}"
    debug "line=$line"
    extension="${filename##*.}"
    debug "extension=$extension"
    gitignore="${repo_root}.gitignore"
    debug "gitignore=$gitignore"

    # ------------------------------------------------
    # Determine the correct git ignore and the path of
    # the file relative to it
    # ------------------------------------------------
    if test $_parent_level -ge 0
    then
        parent=${directory}
        debug "parent=${parent}"
        for i in $(seq 1 $_parent_level)
        do
          parent="$(dirname "$parent")/"
          debug "parent=${parent}"
        done
        root_len=$(echo -n ${repo_root} | wc -m)
        parent_len=$(echo -n ${parent} | wc -m)
        debug "root_len=${root_len}"
        debug "parent_len=${parent_len}"
        if test $root_len -ge $parent_len
        then
            gettextln "WARNING: Parent directory is outside of the repository"
            parent="${repo_root}"
        fi
        rel_directory="${directory#$parent}"
        debug "rel_directory=${rel_directory}"
        gitignore="${parent}.gitignore"
        debug "gitignore=${gitignore}"
    fi

    # ------------------------------------------------
    # Determine the correct line to add to the gitignore
    # based on user inputs
    # ------------------------------------------------
    if test $_ext -eq 1
    then
        line="${rel_directory}*.$extension"
    fi
    if test $_directory -eq 1
    then
        line="${rel_directory}*"
    fi
    if test $_file_anywhere -eq 1
    then
        line="**/$filename"
    fi
    if test $_ext_anywhere -eq 1
    then
        line="**/*.$extension"
    fi
    debug "line=${line}"
    if test $_dry_run -eq 0
    then
        gettextln "Adding $line to $gitignore"
        gettextln $line >> $gitignore
    else
        gettextln "DRY_RUN!! Adding $line to $gitignore"
    fi
}

_ext=0
_directory=0
_file_anywhere=0
_ext_anywhere=0
_parent_level=-1
_dry_run=0

#First, determine the root of the repository
repo_root_raw="$(git rev-parse --show-toplevel)/"
debug "repo_root_raw=$repo_root_raw"
#On windows, this turns to C:\... instead of /c/... from some other commands
repo_root_nix=$(echo -n $repo_root_raw | awk -F":" '/^.*:/ { print "/" tolower($1) $2 }')
debug "repo_root_nix=$repo_root_nix"

while test $# != 0
do
    case "$1" in
    --ext)
         debug "Setting _ext"
        _ext=1
        ;;
    --all-ext)
         debug "Setting _ext_anywhere"
        _ext_anywhere=1
        ;;
    --dir)
         debug "Setting _directory"
        _directory=1
        ;;
    --all-file)
         debug "Setting _file_anywhere"
        _file_anywhere=1
        ;;
    --parent-level=*)
        _parent_level="${1#--parent-level=}"
        debug "Setting _parent_level=$_parent_level"
        if ! echo $_parent_level | grep -q '^[0-9]\+$'
        then
            gettextln "ILLEGAL PARAMETER: -p|--parent-level requires a numerical argument"
            usage
        fi
        ;;
    --dry-run)
         debug "Setting _dry_run"
        _dry_run=1
        ;;
    --)
        only_files_left=1
        ;;
    *)
        if test $only_files_left -eq 1
        then
            add_ignore $1
        fi
        ;;
    esac
    shift
done
exit 0
