#!/bin/bash

USAGE="USAGE: $0 review|patch <path>"
usage() {
	echo "$USAGE"
	exit 2
}
if test -z "$1" -o -z "$2"
then
	usage
fi
case "$1" in
	r|review)
	git send-email --cc spearce@spearce.org --to git@vger.kernel.org "$2"
	;;
	p|patch)
	git send-email --to gitster@pobox.com --cc git@vger.kernel.org "$2"
	;;
	*)
	usage
	;;
esac
