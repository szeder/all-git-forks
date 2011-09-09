#!/bin/sh

(
	c=////////////////////////////////////////////////////////////////
	skel=api-index-skel.asciidoc
	sed -e '/^\/\/ table of contents begin/q' "$skel"
	echo "$c"

	ls api-*.asciidoc |
	while read filename
	do
		case "$filename" in
		api-index-skel.asciidoc | api-index.asciidoc) continue ;;
		esac
		title=$(sed -e 1q "$filename")
		html=${filename%.asciidoc}.html
		echo "* link:$html[$title]"
	done
	echo "$c"
	sed -n -e '/^\/\/ table of contents end/,$p' "$skel"
) >api-index.asciidoc+

if test -f api-index.asciidoc && cmp api-index.asciidoc api-index.asciidoc+ >/dev/null
then
	rm -f api-index.asciidoc+
else
	mv api-index.asciidoc+ api-index.asciidoc
fi
