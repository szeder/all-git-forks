#!/bin/sh

target="$GIT_KCOV"/../../"$(basename "$0")"

test -n "$GIT_IN_KCOV" &&
exec "$target" "$@"

GIT_IN_KCOV=y
export GIT_IN_KCOV

flock -o -w 10 "$GIT_KCOV"/kcov.lock kcov "$GIT_KCOV" "$target" "$@"
