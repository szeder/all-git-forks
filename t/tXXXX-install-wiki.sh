#!/bin/sh

# This script installs or deletes a MediaWiki on your computer.
# It requires a web server with PHP and SQLite running and mediawiki   installed.
# Please set the CONFIGURATION VARIABLES in ./test-gitmw-lib.sh

. ./test-gitmw-lib.sh

help() {
        echo "Usage: "
        echo "  ./tXXXX-install-wiki.sh <install|delete|help>"
        echo "          install: Install a wiki on your computer."
        echo "          delete: Delete the wiki and all its pages and      content"
}

# Argument: install, delete
if [ "$1" = "install" ] ; then
        cmd_install
        exit 0
elif [ "$1" = "delete" ] ; then
        cmd_delete
        exit 0
else
        help
fi

