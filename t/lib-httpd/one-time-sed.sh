#!/bin/sh

if [ -e one-time-sed ]; then
	"$GIT_EXEC_PATH/git-http-backend" | sed "$(cat one-time-sed)"
	rm one-time-sed
else
	"$GIT_EXEC_PATH/git-http-backend"
fi
