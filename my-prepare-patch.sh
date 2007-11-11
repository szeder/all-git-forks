#!/bin/sh

if [ -z "$1" ]; then
	sh "$0" - > git.txt
	exit $?
fi

if [ "$1" != "-" ]; then
	sh "$0" - < "$1" > git.txt
	exit $?
fi

tmp=$(mktemp preppatchXXXXXXX)

cat > $tmp

cat << EOF
[PATCH] 


Signed-off-by: Johannes Schindelin <Johannes.Schindelin@gmx.de>

---

EOF

git apply --stat < $tmp

echo

cat $tmp

rm $tmp

