#!/bin/sh

USAGE="(edit | show) [commit]"
. git-sh-setup

test -n "$3" && usage

test -z "$1" && usage
ACTION="$1"; shift

test -z "$GIT_NOTES_REF" && GIT_NOTES_REF="$(git config core.notesref)"
test -z "$GIT_NOTES_REF" && GIT_NOTES_REF="refs/notes/commits"
export GIT_NOTES_REF

COMMIT=$(git rev-parse --verify --default HEAD "$@") ||
die "Invalid commit: $@"
NOTES_PATH=$COMMIT
case "$GIT_NOTES_SPLIT" in
	[1-9]|[1-4][0-9])
		NOTES_PATH=$( echo $COMMIT | perl -pe 's{^(.{'$GIT_NOTES_SPLIT'})}{$1/}' )
		;;
esac

MESSAGE="$GIT_DIR"/new-notes-$COMMIT
trap '
	test -f "$MESSAGE" && rm "$MESSAGE"
' 0

show_note() {
	COMMIT=$1
	NOTE_PATH=$( git ls-tree --name-only -r $GIT_NOTES_REF | perl -nle '
		$x = $_; s{/}{}g;
		if (m{'$COMMIT'}) {
			print $x;
			exit;
		}
	' )
	[ -n "$NOTE_PATH" ] &&
		git cat-file blob $GIT_NOTES_REF:$NOTE_PATH
}

case "$ACTION" in
edit)
	GIT_NOTES_REF= git log -1 $COMMIT | sed "s/^/#/" > "$MESSAGE"

	GIT_INDEX_FILE="$MESSAGE".idx
	export GIT_INDEX_FILE

	CURRENT_HEAD=$(git show-ref "$GIT_NOTES_REF" | cut -f 1 -d ' ')
	if [ -z "$CURRENT_HEAD" ]; then
		PARENT=
	else
		PARENT="-p $CURRENT_HEAD"
		git read-tree "$GIT_NOTES_REF" || die "Could not read index"
		show_note $COMMIT >> "$MESSAGE"
	fi

	${VISUAL:-${EDITOR:-vi}} "$MESSAGE"

	grep -v ^# < "$MESSAGE" | git stripspace > "$MESSAGE".processed
	mv "$MESSAGE".processed "$MESSAGE"
	if [ -s "$MESSAGE" ]; then
		BLOB=$(git hash-object -w "$MESSAGE") ||
			die "Could not write into object database"
		git update-index --add --cacheinfo 0644 $BLOB $NOTES_PATH ||
			die "Could not write index"
	else
		NOTES_PATH=dummy
	fi

	git ls-files | perl -nle '
		$x = $_; s{/}{}g;
		if (m{'$COMMIT'} and $x ne q{'$NOTES_PATH'}) {
			print $x;
		}' |
		while read path
			do
				git update-index --force-remove $path ||
			    		die "Could not update index"
			done
	
	[ -z "$(git ls-files)" -a -z "$CURRENT_HEAD" ] &&
		die "Will not initialise with empty tree"

	TREE=$(git write-tree) || die "Could not write tree"
	NEW_HEAD=$(echo Annotate $COMMIT | git commit-tree $TREE $PARENT) ||
		die "Could not annotate"
	git update-ref -m "Annotate $COMMIT" \
		"$GIT_NOTES_REF" $NEW_HEAD $CURRENT_HEAD
;;
show)
	show_note $COMMIT
;;
*)
	usage
esac
