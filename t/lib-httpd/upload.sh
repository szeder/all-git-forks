#!/bin/sh

# In part from http://codereview.stackexchange.com/questions/79549/bash-cgi-upload-file

FILES_DIR="www/files"

OLDIFS="$IFS"
IFS='&'
set -- $QUERY_STRING
IFS="$OLDIFS"

echo "query string: '$QUERY_STRING'"  >>upload.log

while test $# -gt 0
do
    key=${1%=*}
    val=${1#*=}

    echo "$key: $val" >>upload.log

    case "$key" in
	"sha1") sha1="$val" ;;
	"type") type="$val" ;;
	"size") size="$val" ;;
	"delete") delete=1 ;;
	*) echo "unknown key '$key'" >>upload.log ;;
    esac

    shift
done

echo "delete: $delete" >>upload.log

case "$REQUEST_METHOD" in
  POST)
    if test "$delete" = "1"
    then
	echo "removing: $FILES_DIR/$sha1-$size-$type" >>upload.log
	rm -f "$FILES_DIR/$sha1-$size-$type"
    else
	mkdir -p "$FILES_DIR"
	cat >"$FILES_DIR/$sha1-$size-$type"
    fi

    echo 'Status: 204 No Content'
    echo
    ;;

  *)
    echo 'Status: 405 Method Not Allowed'
    echo
esac
