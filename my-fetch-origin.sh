#!/bin/sh

HEAD=$(git rev-parse origin)

case "$(cat CVS/Root)" in
*cvs.dev.java.net*)
	EXTRA_CVSPS_OPTS=$EXTRA_CVSPS_OPTS,--no-rlog,--no-cvs-direct;;
esac

git cvsimport -a -i -k -p -b,HEAD$EXTRA_CVSPS_OPTS || exit 1
NEWHEAD=$(git rev-parse origin)
if [ "$HEAD" != "$NEWHEAD" ]; then
	echo "$HEAD" > .git/OLD_ORIGIN
	git log OLD_ORIGIN..origin
fi

