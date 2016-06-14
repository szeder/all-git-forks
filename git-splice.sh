#!/bin/bash
#
# git-splice - splice commits into/out of current branch
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
# Non-interactively splice branch by removing a range of commits from
# within the current branch and/or cherry-picking a range of commits
# into the current branch.
#
# If removal and cherry-picking are performed from the same invocation,
# cherry-picking starts from the point of removal.
#
# N.B. Obviously this command rewrites history!  As with git rebase,
# you should be aware of all the implications of history rewriting
# before using it.  (And actually this command is just a glorified
# wrapper around cherry-pick and rebase -i.)
#
# Examples:
#
#   # Remove commits A..B from the current branch
#   git splice A..B
#
#   # Remove commit A from the current branch
#   git splice A^!
#
#   # Remove commits A..B from the current branch, and cherry-pick
#   # commits C..D at the same point
#   git splice A..B C..D
#
#   # Cherry-pick commits C..D, splicing them in just after commit A
#   git splice A C..D
#
#   # Abort a splice which failed during cherry-pick or rebase
#   git splice --abort
#
#   # Resume a splice after manually fixing conflicts caused by
#   # cherry-pick or rebase
#   git splice --continue

me=`basename $0`
git_dir=`git rev-parse --git-dir` || exit 1
splice_dir="$git_dir/splice"
base_file="$splice_dir/base"
branch_file="$splice_dir/branch"
insert_todo="$splice_dir/insert-todo"
remove_todo="$splice_dir/remove-todo"
rebase_cancelled="$splice_dir/rebase-cancelled"
TMP_BRANCH="tmp/splice"
export PS4='${BASH_SOURCE/$HOME/~}@${LINENO}(${FUNCNAME[0]}): '

main ()
{
    parse_opts "$@"

    if [ -n "$in_progress" ]; then
        if in_progress; then
            echo "Splice in progress: $reason"
            exit 0
        else
            echo "Splice not in progress"
            exit 1
        fi
    fi

    if [ -n "$abort" ] || [ -n "$continue" ] || [ -n "$rebase_edit" ]; then
        ensure_splice_in_progress
    else
        # Needs to happen before parse_args(), otherwise the in-flight
        # files will already exist.
        ensure_splice_not_in_progress
    fi

    parse_args "${ARGV[@]}"

    if [ -n "$rebase_edit" ]; then
        # We're being invoked by git rebase as the rebase todo list editor,
        # rather than by the user.  This mode is for internal use only.
        rebase_edit
        return
    fi

    if [ -n "$abort" ]; then
        splice_abort
        return
    fi

    # Handle both normal execution and --continue
    splice
}

splice ()
{
    base="$(<$base_file)"
    branch="$(<$branch_file)"

    validate_base

    if valid_ref "$TMP_BRANCH"; then
        if [ -z "$continue" ]; then
            abort "BUG: $TMP_BRANCH exists but no --continue"
        fi

        if ! on_branch "$TMP_BRANCH"; then
            : "Presumably on a detached head in the middle of a rebase"
        fi
    else
        echo git checkout -q -B "$TMP_BRANCH" "$base"
        git checkout -q -B "$TMP_BRANCH" "$base"
    fi

    if [ -s "$insert_todo" ]; then
        if cherry_pick_active; then
            if ! git cherry-pick --continue; then
                error_and_pause "git cherry-pick --continue failed!"
            fi
        else
            reason="cat $insert_todo | xargs git cherry-pick"
            if ! cat $insert_todo | xargs -t git cherry-pick; then
                error_and_pause "git cherry-pick failed!"
            fi
            rm "$insert_todo"
        fi
    fi

    if rebase_active; then
        args=( --continue )
    else
        args=( -i --onto "$TMP_BRANCH" "$base" "$branch" )
        if [ "$base" = "$branch" ]; then
            skip_rebase=y
            echo git checkout -B "$branch" "$TMP_BRANCH"
            git checkout -B "$branch" "$TMP_BRANCH"
        fi
    fi

    if [ -z "$skip_rebase" ]; then
        export GIT_SEQUENCE_EDITOR="$0 $debug --rebase-edit"
        echo git rebase "${args[@]}"
        git rebase "${args[@]}" | tweak_rebase_error
        if [ "${PIPESTATUS[0]}" -ne 0 ]; then
            if [ -e "$rebase_cancelled" ]; then
                : "happens if there were no commits (left) to rebase"
		git reset --hard "$TMP_BRANCH"
                rm "$rebase_cancelled"
            else
                error_and_pause "git rebase ${args[*]} failed!"
            fi
        fi
    fi

    git branch -d "$TMP_BRANCH"
    rm -rf "$splice_dir"
}

tweak_rebase_error ()
{
    sed -e 's/git rebase \(--continue\|--abort\)/git splice \1/g'
}

# FIXME: duplicated in both spice and transplant
valid_ref ()
{
    git rev-parse --quiet --verify "$@" >/dev/null
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

validate_base ()
{
    if [ -z "$base" ]; then
        abort "BUG: base should not be empty"
    fi

    if ! valid_ref "$base"; then
        cleanup
        abort "Base commit $base was not valid"
    fi
}

error_and_pause ()
{
    warn "$*"
    warn "When you have resolved this problem, run \"git $workflow --continue\","
    warn "or run \"git $workflow --abort\" to abandon the splice."
    exit 1
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

    if [ -d "$splice_dir" ]; then
        reason="$splice_dir exists"
        return 0
    fi

    reason=
    return 1
}

cleanup ()
{
    aborted=

    if [ -e "$insert_todo" ]; then
        # Can we be sure that the in-flight cherry-pick was started by
        # git splice?  Probably, because otherwise
        # ensure_cherry_pick_not_in_progress should have prevented us
        # from reaching this point in the code.
        if cherry_pick_active; then
            git cherry-pick --abort
        fi

        rm "$insert_todo"
        aborted=y
    fi

    if [ -e "$remove_todo" ]; then
        if rebase_active; then
            git rebase --abort
        fi

        rm "$remove_todo"
        aborted=y
    fi

    if valid_ref "$TMP_BRANCH"; then
        if on_branch "$TMP_BRANCH"; then
            git checkout "$(<$branch_file)"
        fi

        git branch -d "$TMP_BRANCH"
        aborted=y
    fi

    if [ -d "$splice_dir" ]; then
        rm -rf "$splice_dir"
        aborted=y
    fi
}

splice_abort ()
{
    cleanup

    if [ -z "$aborted" ]; then
        abort "No splice in progress"
    fi
}

# FIXME: duplicated in both spice and transplant
head_ref ()
{
    git symbolic-ref --short -q HEAD
}

on_branch ()
{
    [ "`head_ref`" = "$1" ]
}

ensure_splice_in_progress ()
{
    if ! in_progress; then
        abort "Splice not in progress"
    fi
}

ensure_splice_not_in_progress ()
{
    for file in "$insert_todo" "$remove_todo"; do
        if [ -e "$file" ]; then
            in_progress_error "$file already exists."
        fi
    done

    ensure_cherry_pick_not_in_progress
    ensure_rebase_not_in_progress

    if on_branch "$TMP_BRANCH"; then
        die "BUG: on $TMP_BRANCH branch, but no splice in progress?"
    fi

    if valid_ref "$TMP_BRANCH"; then
        die "BUG: $TMP_BRANCH branch exists, but no splice in progress?"
    fi
}

# FIXME: duplicated in both spice and transplant
in_progress_error ()
{
    cat <<EOF >&2
$*

git splice already in progress; please complete it, or run

  git splice --abort
EOF
    exit 1
}

# FIXME: duplicated in both spice and transplant
ensure_cherry_pick_not_in_progress ()
{
    if cherry_pick_active; then
        abort "Can't start git splice when there is a cherry-pick in progress"
    fi
}

# FIXME: duplicated in both spice and transplant
ensure_rebase_not_in_progress ()
{
    if rebase_active; then
        warn "Can't start git splice when there is a rebase in progress."

        # We know this will fail; we run it because we want to output
        # the same error message which git-rebase uses to tell the user
        # to finish or abort their in-flight rebase.
        git rebase
        exit 1
    fi
}

rebase_edit ()
{
    if ! [ -e "$rebase_todo" ]; then
        abort "BUG: $me invoked in rebase edit mode, but $rebase_todo was missing"
    fi

    if [ -e "$remove_todo" ]; then
        grep -v -f "$remove_todo" "$rebase_todo" > "$rebase_todo".new
        if [ -n "$debug" ]; then
            echo -e "-------------------\n$rebase_todo"
            cat "$rebase_todo"
            echo -e "-------------------\n$remove_todo"
            cat "$remove_todo"
            echo -e "-------------------\n$rebase_todo.new"
            cat "$rebase_todo.new"
        fi
        mv "$rebase_todo".new "$rebase_todo"
    fi

    if ! grep '^ *[a-z]' "$rebase_todo"; then
        echo "Nothing left to rebase; cancelling."
        >"$rebase_todo"
        touch "$rebase_cancelled"
    fi
}

# FIXME: duplicated in both spice and transplant
warn ()
{
    echo >&2 "$*"
}

# FIXME: duplicated in both spice and transplant
die ()
{
    echo >&2 "$*"
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
  $me REMOVE-RANGE
  $me REMOVE-RANGE CHERRY-PICK-RANGE
  $me INSERTION-POINT CHERRY-PICK-RANGE
  $me REMOVE-RANGE-ARGS ... -- CHERRY-PICK-RANGE-ARGS ...
  $me OPTION

Options:
  -h, --help         Show this help and exit
      --abort        Abort an in-progress splice
      --continue     Continue an in-progress splice
      --in-progress  Exit 0 iff any splice is in progress

REMOVE-RANGE and CHERRY-PICK-RANGE are single shell words specifying
commit ranges in the standard format accepted by "git rev-parse", e.g.

  A..B
  A...B
  A^!   (just commit A)

INSERTION-POINT is a commitish in the standard format accepted
by "git rev-parse".

REMOVE-RANGE specifies the range of commits to remove from the current
branch, and CHERRY-PICK-RANGE specifies the range to insert at the
point where REMOVE-RANGE previously existed, or just after
INSERTION-POINT.

It is possible to pass multi-word specifications for both the removal
and insertion ranges, in which case they are passed to "git rev-list"
to calculate the commits to remove or cherry-pick.  For this you need
to terminate REMOVE-RANGE-ARGS with "--", e.g.

  # remove all commits since 11am this morning mentioning "foo"
  git splice --since=11am --grep="foo" --
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
    workflow=splice

    while [ $# != 0 ]; do
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
            # for internal use only
            --rebase-edit)
                rebase_edit=yes
                rebase_todo="$2"
                shift 2
                ;;
            --transplant)
                workflow=transplant
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

    ARGV=( "$@" )
}

parse_args ()
{
    if [ -n "$abort" ] || [ -n "$continue" ] ||
       [ -n "$in_progress" ] || [ -n "$rebase_edit" ]; then
        return
    fi

    count=$#
    for word in "$@"; do
        if [ "$word" = '--' ]; then
            multi_word=yes
            count=$(( count - 1 ))
            break
        fi
    done

    if [ $count -eq 0 ]; then
        usage "You must specify at least one range to remove or insert."
    fi

    if [ -z "$multi_word" ]; then
        if [ $# -ge 1 ]; then
            remove_range_or_insertion_base=( "$1" )
        fi

        if [ $# -eq 2 ]; then
            insert_range=( "$2" )
        elif [ $# -ge 2 ]; then
            usage "Use of multiple words in the removal or insertion" \
                  "ranges requires the -- separator"
        fi
    else
        remove_range_or_insertion_base=()
        for word in "$@"; do
            if [ "$word" = '--' ]; then
                shift
                insert_range=( "$@" )
                break
            fi
            remove_range_or_insertion_base+=( "$word" )
            shift
        done
    fi

    if ! parsed=(
            `git rev-parse "${remove_range_or_insertion_base[@]}" 2>/dev/null`
        )
    then
        cleanup
        abort "Failed to parse ${remove_range_or_insertion_base[*]}"
    fi

    mkdir -p "$splice_dir"

    if ! head_ref > "$branch_file"; then
        rm "$branch_file"
        abort "Cannot run $me on detached head"
    fi

    if [ "${#parsed[@]}" -eq 1 ]; then
        : "$remove_range_or_insertion_base must be an insertion point"
        insertion_point="$remove_range_or_insertion_base"
        if [ "${#insert_range[@]}" -eq 0 ]; then
            cleanup
            usage "You provided an insertion point but no range to cherry-pick."
        fi
        echo "$insertion_point" >"$base_file"
    else
        : "$remove_range_or_insertion_base must be a removal range"
        # FIXME: there's no guarantee that the range specifies commits
        # in the branch.  If the base ends up being outside the branch,
        # this could actually *introduce* commits!
        remove_range=( "${remove_range_or_insertion_base[@]}" )
        git rev-list --abbrev-commit "${remove_range[@]}" | \
            sed 's/^/pick /' >"$remove_todo"
        earliest=$( tail -n1 "$remove_todo" | sed 's/^pick \(.\+\)/\1/' )
        echo "Earliest commit in $remove_range is $earliest"
        echo "${earliest}^" >"$base_file"
    fi

    if [ "${#insert_range[@]}" -gt 0 ]; then
        if ! valid_range "${insert_range[@]}"; then
            cleanup
            abort "Failed to parse ${insert_range[*]}"
        fi

        if [ "${#insert_range[@]}" -eq 1 ]; then
            echo "${insert_range[@]}" >"$insert_todo"
        else
            git rev-list --reverse "${insert_range[@]}" >"$insert_todo"
        fi
    fi
}

main "$@"
