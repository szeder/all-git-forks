#!/bin/sh

# from http://codereview.stackexchange.com/questions/79549/bash-cgi-upload-file

OLDIFS="$IFS"
IFS='&'
set -- $QUERY_STRING
IFS="$OLDIFS"

while test $# -gt 0
do
    key=${1%=*}
    val=${1#*=}

    echo "$key: $val" >>upload.log

    case "$key" in
	filename) filename="$val" ;;
    esac

    shift
done

case "$REQUEST_METHOD" in
  POST)
    cat >"$filename"

    echo "query: $QUERY_STRING" >query_string.txt

    echo 'Status: 204 No Content'
    echo
    ;;

  *)
    echo 'Status: 405 Method Not Allowed'
    echo
esac
