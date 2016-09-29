#!/bin/sh

# from http://codereview.stackexchange.com/questions/79549/bash-cgi-upload-file

case "$REQUEST_METHOD" in
  POST)
    (
        # Read and echo
        while read nextline ; do
            echo "$nextline"
        done

    ) > hello_apache

    echo 'Status: 204 No Content'
    echo
    ;;

  *)
    echo 'Status: 405 Method Not Allowed'
    echo
esac
