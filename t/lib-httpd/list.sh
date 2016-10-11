#!/bin/sh

FILES_DIR="www/files"

OLDIFS="$IFS"
IFS='&'
set -- $QUERY_STRING
IFS="$OLDIFS"

CUR_LOG="list.log"

echo "query string: '$QUERY_STRING'"  >>"$CUR_LOG"

echo 'Status: 200 OK'
echo

if test -d "$FILES_DIR"
then
    ls "$FILES_DIR" | tr '-' ' '
fi

