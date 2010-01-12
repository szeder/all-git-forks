#!/bin/sh
# 
# Split a repository into a submodule and main module, with history
#
# Copyright 2009 Trustees of Dartmouth College
# License: GNU General Public License, version 2 or later

USAGE="[--url submodule_repo_url] submodule_dir [alternate_dir...]"

OPTIONS_SPEC=
. git-sh-setup
require_work_tree

# Set up our temporary directory.  We export these variables because we
# want to use them from scripts passed to 'git filter-branch'.  We can't
# simply substitute these variable values into the text of the scripts we
# pass to 'git filter-branch', because the filenames may contain spaces,
# which would get mangled.  Thanks to Johannes Schindelin for this idea.
export GIT_SPLIT_TEMP_DIR="$GIT_DIR/.git_split"
export GIT_SPLIT_MAP_DIR="$GIT_SPLIT_TEMP_DIR/map"
rm -rf "$GIT_SPLIT_TEMP_DIR" &&
mkdir -p "$GIT_SPLIT_MAP_DIR" || exit

# Parse our command-line arguments.
mkdir "$GIT_SPLIT_TEMP_DIR/dirs" || exit
dir_count=0
while test $# -ne 0; do
	case "$1" in
	--)
		shift
		break
		;;
	--url)
                shift
                test $# -ne 0 || die "Must supply argument to --url"
		sub_url="$1"
		shift
		;;
	-*)
		die "Unknown option: $1"
		;;
	*)
		# Use the first specified directory as the subrepository
		# name.
		if test "$dir_count" -eq 0; then
			sub_path="$1"
		fi

		# There's no good way to pass an array of filenames
		# containing spaces to our subprocesses, so let's cheat
		# shamelessly and create an "array" of files on disk.
		printf '%s' "$1" > "$GIT_SPLIT_TEMP_DIR/dirs/$dir_count"
		dir_count=$(($dir_count + 1))
                shift
		;;
	esac
done

# We should have at least one directory listed on the command line.
test "$dir_count" -ge 1 || usage

# Default the repository URL to something based on the repository path.
if test -z "$sub_url"; then
	sub_url="../$sub_path"
fi

# TODO: Pass remaining arguments to rev-parse, defaulting to --all.
revs="--all"

# More variables for our subprocesses.
export GIT_SPLIT_DIR_COUNT="$dir_count"
export GIT_SPLIT_SUB_PATH="$sub_path"
export GIT_SPLIT_SUB_URL="$sub_url"

# Make sure our environment is sane.
test "$(is_bare_repository)" = false ||
	die "Cannot run submodule split in a bare repository"
git diff-files --ignore-submodules --quiet &&
	git diff-index --cached --quiet HEAD -- ||
	die "Cannot split out a submodule with a dirty working directory."
(cd "$sub_path" &&
	test "$GIT_DIR" = "$(git rev-parse --git-dir)" ||
	die "$sub_path is already in a submodule")



#--------------------------------------------------------------------------
# Create the new submodule

# Create a new repository at the last known address of our submodule.  We
# initially share our objects with our parent repository.
src_repo="$(pwd)"
sub_repo_temp="$GIT_SPLIT_TEMP_DIR/s" && mkdir "$sub_repo_temp" &&
(cd "$sub_repo_temp" &&
	git init &&
	git remote add origin --mirror "$src_repo" &&
	echo "$src_repo/.git/objects" > .git/objects/info/alternates &&
	git fetch --update-head-ok &&
	git remote rm origin &&
	git read-tree -u -m HEAD) || exit

index_filter=$(cat << \EOF
map_info="$GIT_SPLIT_MAP_DIR/$GIT_COMMIT"

# Check for the submodule in all possible locations.
i=0
while test "$i" -lt "$GIT_SPLIT_DIR_COUNT"; do
	candidate="$(cat "$GIT_SPLIT_TEMP_DIR/dirs/$i")" || exit
	if git rev-parse -q --verify "$GIT_COMMIT:$candidate"; then
		# Borrowed from git filter-branch.
		err="$(git read-tree -i -m "$GIT_COMMIT:$candidate" 2>&1)" ||
			die "$err"
	        printf '%s' "$candidate" > "$map_info-dir"
		break
	fi
	i=$(($i + 1))
done
EOF
)

commit_filter=$(cat << \EOF
map_info="$GIT_SPLIT_MAP_DIR/$GIT_COMMIT"
if test -f "$map_info-dir"; then
	new_commit="$(git_commit_non_empty_tree "$@")" || exit
	echo "$new_commit"
	echo "$new_commit" > "$map_info-submodule-commit" ||
		die "Can't record the commit ID of the new commit"
else
	skip_commit "$@"
fi
EOF
)

# Run our filters, repack the results as a standalone repository with no
# extra history, and check out HEAD.
(cd "$sub_repo_temp" &&
	git filter-branch --index-filter "$index_filter" \
		--commit-filter "$commit_filter" -- "$revs" &&
	rm -rf .git/refs/original &&
	git reflog expire --expire="now" --all &&
	git repack -a -d &&
	git clean -qdf &&
	rm .git/objects/info/alternates) || exit

#--------------------------------------------------------------------------
# Create the new superproject

index_filter=$(cat << \EOF
map_info="$GIT_SPLIT_MAP_DIR/$GIT_COMMIT"

# Only update the index if the submodule is present in this revision.
if test -f "$map_info-dir"; then
	dir="$(cat "$map_info-dir")" || exit

	# Splice the repo into the tree.
	test -f "$map_info-submodule-commit" ||
		die "Can't find map for $GIT_COMMIT"
	git rm -q --cached -r "$dir" || exit
	subcommit="$(cat "$map_info-submodule-commit")" || exit
	echo "160000 $subcommit	$dir" |
		git update-index --index-info || exit

	# Either update the old .gitmodules file, or make a new one.
	gitmodules="$GIT_SPLIT_TEMP_DIR/gitmodules"
	touch "$gitmodules"	  # Assure gitmodules exists
	if git rev-parse -q --verify "$GIT_COMMIT:.gitmodules"; then
		git cat-file blob "$GIT_COMMIT:.gitmodules" > "$gitmodules" ||
			exit
	fi
	subsection=submodule."$GIT_SPLIT_SUB_PATH"
	git config -f "$gitmodules" "$subsection".path "$dir"
	git config -f "$gitmodules" "$subsection".url "$GIT_SPLIT_SUB_URL"

	# Write the new .gitmodules file into the tree.
	new_obj="$(git hash-object -t blob -w "$gitmodules")" ||
		die "Error adding new .gitmodules file to tree"
	git update-index --add --cacheinfo 100644 "$new_obj" .gitmodules || exit
fi
EOF
)

# Run our filter.
git filter-branch --index-filter "$index_filter" -- "$revs" || exit

# Move our submodule into place.  This has to wait until last, because
# we want to keep the tree clean until after the final git filter, and we
# need to have a place to put the new submodule.
rmdir "$sub_path"
test -d "$sub_path" && die "submodule $sub_path was not actually deleted"
mv "$sub_repo_temp" "$sub_path" || exit

exit 0
