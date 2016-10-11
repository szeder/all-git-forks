#!/bin/sh

FILES_DIR="www/files"

OLDIFS="$IFS"
IFS='&'
set -- $QUERY_STRING
IFS="$OLDIFS"

CUR_LOG="list.log"

echo "query string: '$QUERY_STRING'"  >>"$CUR_LOG"

while test $# -gt 0
do
    key=${1%=*}
    val=${1#*=}

    echo "$key: $val" >>"$CUR_LOG"

    case "$key" in
	"sha1") sha1="$val" ;;
	*) echo "unknown key '$key'" >>"$CUR_LOG" ;;
    esac

    shift
done


echo 'Status: 200 OK'
echo

if test -d "$FILES_DIR"
then
    if test -n "$sha1"
    then
	cat "$FILES_DIR/$sha1"-*
    else
	ls "$FILES_DIR" | tr '-' ' '
    fi
fi

