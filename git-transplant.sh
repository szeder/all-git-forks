#!/bin/bash
#
# git-transplant - transplant commits into/out of current branch
# Copyright (c) 2016 Adam Spiers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ---------------------------------------------------------------------
#
# Non-interactively transplant a range of commits from the current
# branch onto or into another branch.
#
# N.B. Obviously this command rewrites history!  As with git rebase,
# you should be aware of all the implications of history rewriting
# before using it.  (Actually this command is just a glorified
# wrapper around git-splice.)
#
# Examples:
#
#   # Move commits A..B from the current branch onto branch X
#   git transplant A..B X
#
#   # Move commits A..B from the current branch into branch X after commit C
#   git transplant --after=C A..B X
#
#   # Create a new branch X starting at ref Y, then
#   # move commits A..B from the current branch onto X
#   git transplant --new-from=Y C A..B X
#
#   # Abort a transplant which failed during cherry-pick or rebase
#   git transplant --abort
#
#   # Resume a transplant after manually fixing conflicts caused by
#   # cherry-pick or rebase
#   git transplant --continue
#
# FIXME:
#
#  - correctly handle when range contains commits outside the current branch

me=`basename $0`
git_dir=`git rev-parse --git-dir` || exit 1
transplant_dir="$git_dir/transplant"
src_branch_file="$transplant_dir/src-branch"
dest_branch_file="$transplant_dir/dest-branch"
dest_branch_created_file="$transplant_dir/dest-branch-created"
dest_branch_orig_sha_file="$transplant_dir/dest-branch-orig-sha"
after_file="$transplant_dir/after"
insert_todo="$transplant_dir/insert-todo"
remove_todo="$transplant_dir/remove-todo"
splice_file="$transplant_dir/splice-in-progress"
export PS4='${BASH_SOURCE/$HOME/~}@${LINENO}(${FUNCNAME[0]}): '

main ()
{
    parse_opts "$@"

    if [ -n "$in_progress" ]; then
        if in_progress; then
            echo "Transplant in progress: $reason"
            exit 0
        else
            echo "Transplant not in progress"
            exit 1
        fi
    fi

    if [ -n "$abort" ] || [ -n "$continue" ]; then
        ensure_transplant_in_progress
    else
        # Needs to happen before parse_args(), otherwise the in-flight
        # files will already exist.
        ensure_transplant_not_in_progress
        ensure_cherry_pick_not_in_progress
        ensure_rebase_not_in_progress
    fi

    parse_args "${ARGV[@]}"

    src_branch="$(<$src_branch_file)"
    dest_branch="$(<$dest_branch_file)"

    if [ -n "$abort" ]; then
        transplant_abort
        return
    fi

    # Handle both normal execution and --continue
    transplant
}

transplant ()
{
    prep_dest_branch

    if [ -s "$insert_todo" ]; then
        insert_range
    fi

    # I can't think why the remove file would be empty,
    # but you never know.
    if [ -s "$remove_todo" ]; then
        remove_range
    fi
}

prep_dest_branch ()
{
    if [ -n "$continue" ]; then
        return
    fi

    if [ -n "$new_from" ]; then
        if git checkout -b "$dest_branch" "$new_from"; then
            touch "$dest_branch_created_file"
        else
            cleanup
            abort "Couldn't create $dest_branch at $new_from"
        fi
    else
        if ! git checkout "$dest_branch"; then
            cleanup
            abort "Couldn't checkout $dest_branch"
        fi
    fi
}

announce_action ()
{
    set +x
    echo "###############################################################"
    echo "# $action"
    if [ -n "$debug" ]; then
        set -x
    fi
}

insert_range ()
{
    if [ -s "$after_file" ]; then
        action="insert into $dest_branch"
    else
        action="insert onto $dest_branch"
    fi

    announce_action

    if [ -z "$continue" ]; then
        if [ -s "$after_file" ]; then
            insertion_point="$(<$after_file)"
        else
            # Of course we could just cherry-pick here, but this would
            # mean handling an extra path in the workflow.  By always
            # performing insertion via splice, we simplify handling of the
            # workflow, especially when things go wrong.
            insertion_point="$dest_branch"
        fi

        args=( "$insertion_point" -- "$(<$insert_todo)" )
    else
        args=( --continue )

        if [ -e "$splice_file" ]; then
            check_splice_file_action
        else
            warn "$splice_file was missing; continuing splice anyway ..."
        fi
    fi

    echo "$action" >"$splice_file"
    if git_splice "${args[@]}"; then
        rm "$insert_todo" "$splice_file"
        return 0
    else
        ret=$?
    fi

    if git_splice --in-progress >/dev/null; then
        # splice must have failed due to conflicts.  It will have
        # already output the correct error message, so just wait
        # for the user to do transplant --abort or --continue.
        exit $ret
    fi

    # Otherwise splice must have failed early on, before starting the
    # workflow.  It will have already output the correct error
    # message, so just clean up and bail.
    cleanup
    exit $ret
}

git_splice ()
{
    echo git splice $debug --transplant "$@"
    git splice $debug --transplant "$@"
}

check_splice_file_action ()
{
    if ! grep -q "$action" "$splice_file"; then
        abort "$splice_file contained:

`cat $splice_file`

but was expecting:

$action"
    fi
}

remove_range ()
{
    if ! rebase_active; then
        git checkout "$src_branch"
    fi

    action="remove from $src_branch"
    if [ -z "$continue" ] && [ -e "$splice_file" ]; then
        abort "BUG: $splice_file should not exist at this point"
    fi

    announce_action

    if remove_command; then
        rm -rf "$transplant_dir"
        exit 0
    else
        # splice must have failed, but we don't care whether that was
        # due to conflicts or something else - either way, we're still
        # in the middle of the transplant workflow, it will have
        # already output the correct error message, so just wait for
        # the user to do transplant --abort or --continue.
        exit $?
    fi
}

remove_command ()
{
    if [ -n "$continue" ] && [ -e "$splice_file" ]; then
        # We must be continuing from a previously failed removal,
        # since once the insertion completed, $splice_file gets
        # removed.
        check_splice_file_action
        git_splice --continue
    else
        # We're either continuing from a previously failed insertion,
        # or this is the first attempt to run this workflow.  Either
        # way, this is the first attempt at the removal phase.
        echo "$action" >"$splice_file"
        cat "$remove_todo" | xargs -t git splice $debug --transplant
    fi
}

# FIXME: duplicated in both spice and transplant
valid_ref ()
{
    git rev-parse --quiet --verify "$@" >/dev/null
}

valid_branch ()
{
    case $( git rev-parse --symbolic-full-name "$1" ) in
        refs/heads/*)
            return 0
            ;;
    esac

    return 1
}

# FIXME: duplicated in both spice and transplant
valid_range ()
{
    git rev-parse "$@" >/dev/null 2>&1
}

# FIXME: duplicated in both spice and transplant
cherry_pick_active ()
{
    # Ideally git rebase would have some plumbing for this, so
    # we wouldn't have to assume knowledge of internals.
    valid_ref CHERRY_PICK_HEAD
}

# FIXME: duplicated in both spice and transplant
rebase_active ()
{
    # Ideally git rebase would have some plumbing for this, so
    # we wouldn't have to assume knowledge of internals.  See:
    # http://stackoverflow.com/questions/3921409/how-to-know-if-there-is-a-git-rebase-in-progress
    [ -e "$git_dir/rebase-merge" ] ||
    [ -e "$git_dir/rebase-apply" ]
}

in_progress ()
{
    if [ -e "$insert_todo" ]; then
        reason="$insert_todo exists"
        return 0
    fi

    if [ -e "$remove_todo" ]; then
        reason="remove_todo exists"
        return 0
    fi

    if [ -e "$splice_file" ] && git splice --in-progress >/dev/null; then
        reason="splice started by transplant is in progress"
        return 0
    fi

    if [ -d "$transplant_dir" ]; then
        reason="$transplant_dir exists"
        return 0
    fi

    reason=
    return 1
}

cleanup ()
{
    aborted=

    if [ -e "$insert_todo" ]; then
        rm "$insert_todo"
        aborted=y
    fi

    if [ -e "$remove_todo" ]; then
        rm "$remove_todo"
        aborted=y
    fi

    if [ -e "$splice_file" ] && git splice --in-progress >/dev/null; then
        git splice --abort
        aborted=y
    fi

    if [ -n "$src_branch" ] && ! on_branch "$src_branch"; then
        git checkout "$src_branch"
        aborted=y
    fi

    if [ -e "$dest_branch_created_file" ]; then
        if valid_ref "$dest_branch"; then
            git branch -D "$dest_branch"
            aborted=y
        fi
    elif [ -e "$dest_branch_orig_sha_file" ]; then
        orig_dest_sha="$(<$dest_branch_orig_sha_file)"
        if [ `git rev-parse "$dest_branch"` != "$orig_dest_sha" ]
        then
            git branch -f "$dest_branch" "$orig_dest_sha"
            aborted=y
        fi
    fi

    if [ -d "$transplant_dir" ]; then
        rm -rf "$transplant_dir"
        aborted=y
    fi
}

transplant_abort ()
{
    cleanup

    if [ -z "$aborted" ]; then
        abort "No transplant in progress"
    fi
}

head_ref ()
{
    git symbolic-ref --short -q HEAD
}

# FIXME: duplicated in both spice and transplant
on_branch ()
{
    [ "`head_ref`" = "$1" ]
}

ensure_transplant_in_progress ()
{
    if ! in_progress; then
        abort "Transplant not in progress"
    fi
}

ensure_transplant_not_in_progress ()
{
    if in_progress; then
        in_progress_error "$reason"
    fi
}

# FIXME: duplicated in both spice and transplant
in_progress_error ()
{
    cat <<EOF >&2
$*

git transplant already in progress; please complete it, or run

  git transplant --abort
EOF
    exit 1
}

# FIXME: duplicated in both spice and transplant
ensure_cherry_pick_not_in_progress ()
{
    if cherry_pick_active; then
        abort "Can't start git transplant when there is a cherry-pick in progress"
    fi
}

# FIXME: duplicated in both spice and transplant
ensure_rebase_not_in_progress ()
{
    if rebase_active; then
        warn "Can't start git transplant when there is a rebase in progress."

        # We know this will fail; we run it because we want to output
        # the same error message which git-rebase uses to tell the user
        # to finish or abort their in-flight rebase.
        git rebase
        exit 1
    fi
}

# FIXME: duplicated in both spice and transplant
warn ()
{
    echo >&2 -e "$*"
}

# FIXME: duplicated in both spice and transplant
die ()
{
    echo >&2 -e "$*"
    exit 1
}

# FIXME: duplicated in both spice and transplant
abort ()
{
    die "$*; aborting."
}

usage ()
{
    # Call as: usage [EXITCODE] [USAGE MESSAGE]
    exit_code=1
    if [[ "$1" == [0-9] ]]; then
        exit_code="$1"
        shift
    fi

    cat <<EOF >&2
Usage:
  $me [OPTIONS] TRANSPLANT-RANGE DEST-BRANCH

Options:
  -h, --help          Show this help and exit
  -a, --after=REF     Transplant into inside DEST-BRANCH after the given ref
  -n, --new-from=REF  First create DEST-BRANCH starting at REF
      --abort         Abort an in-progress transplant
      --continue      Continue an in-progress transplant

TRANSPLANT-RANGE specifies a commit range in the standard format
accepted by "git rev-parse", e.g.

  A..B
  A...B
  A^!   (just commit A)

This range will be removed from the current branch and then
transplanted to DEST-BRANCH.
EOF

    if [ -n "$1" ]; then
        echo >&2
        echo >&2 "ERROR: $*"
    fi

    exit "$exit_code"
}

parse_opts ()
{
    ORIG_ARGV=( "$@" )

    while [ $# != 0 ]; do
        : "next argument: $1"
        case "$1" in
            -h|--help)
                usage 0
                ;;
            -v|--version)
                echo "$me $VERSION"
                ;;
            -d|--debug)
                debug=--debug
                echo >&2 "#-------------------------------------------------"
                echo >&2 "# Invocation: $0 ${ORIG_ARGV[@]}"
                set -x
                shift
                ;;
            --continue)
                continue=yes
                shift
                ;;
            --abort)
                abort=yes
                shift
                ;;
            --in-progress)
                in_progress=yes
                shift
                ;;
            -a|--after)
                after="$2"
                shift 2
                ;;
            --after=*)
                after="${1#--after=}"
                shift
                ;;
            -n|--new-from)
                new_from="$2"
                shift 2
                ;;
            --new-from=*)
                new_from="${1#--new-from=}"
                shift
                ;;
            *)
                break
                ;;
        esac
    done

    if echo "$continue$abort$in_progress" | grep -q yesyes; then
        usage "You must only select one of --abort, --continue, and --in-progress."
    fi

    if [ -n "$after" ] && [ -n "$new_from" ]; then
        usage "You must only select one of --after and --new-from."
    fi

    ARGV=( "$@" )
}

parse_args ()
{
    if [ -n "$abort" ] || [ -n "$continue" ] || [ -n "$rebase_edit" ]; then
        return
    fi

    if [ $# -ne 2 ]; then
        usage "Incorrect number of arguments."
    fi

    range="$1"
    dest_branch="$2"

    if ! parsed=(
            `git rev-parse "${range[@]}" 2>/dev/null`
        )
    then
        cleanup
        abort "Failed to parse ${range[*]}"
    fi

    if [ "${#parsed[@]}" -eq 1 ]; then
        cleanup
        usage "TRANSPLANT_RANGE must not be a reference to a single commit."
    fi

    mkdir -p "$transplant_dir"

    if ! head_ref > "$src_branch_file"; then
        cleanup
        abort "Cannot run $me on detached head"
    fi

    head=$(<$src_branch_file)

    echo "${range[@]}" | sed "s/\(^\|[^_]\)HEAD/\1${head//\//\\/}/g" > "$insert_todo"
    cp "$insert_todo" "$remove_todo"

    if [ -z "$new_from" ]; then
        if ! valid_ref "$dest_branch"; then
            cleanup
            abort "Failed to parse $dest_branch"
        fi

        if ! valid_branch "$dest_branch"; then
            cleanup
            abort "Destination $dest_branch isn't a branch"
        fi
    else
        if valid_ref "$dest_branch"; then
            cleanup
            abort "$dest_branch should not already exist when using --new-from"
        fi
    fi

    for ref in "$after" "$new_from"; do
        if [ -n "$ref" ] && ! valid_ref "$ref"; then
            cleanup
            abort "Failed to parse $ref"
        fi
    done

    echo "$dest_branch" > "$dest_branch_file"
    if [ -z "$new_from" ]; then
        git rev-parse "$dest_branch" > "$dest_branch_orig_sha_file"
    fi

    if [ -n "$after" ]; then
        echo "$after" > "$after_file"
    fi
}

main "$@"
