# Shell library for testing git with watchman
#
# Copyright (c) 2015 Twitter Inc.
#

# create/return a temp directory which is specific to the
# current TRASH_DIRECTORY but safely out of harm's way in /tmp
supertrash () {
	local dir
	dir="/tmp/watchman_trash.$(echo "$TRASH_DIRECTORY" | openssl sha1)"
	install -d "$dir"
	echo "$TRASH_DIRECTORY" >"${dir}/env_trash"
	echo "$dir"
}

setup_watchman () {
	WATCHMAN_HOME="$(mktemp -d /tmp/watchman_home.XXXXXXXX)"
	export WATCHMAN_HOME
	echo "$WATCHMAN_HOME" >>"$(supertrash)/.watchman_homes"
}

# enable watchman for the repository path specified, or the cwd
enable_watchman () {
	if [ ! -n "$WATCHMAN_HOME" ] ; then
		error "watchman is not set up for testing; you probably need to call setup_watchman ($WATCHMAN_HOME)"
	elif [ ! -x "$(dirname "$(which git)")/watchman" ] ; then
		error "watchman not found; you might need to set GIT_TEST_INSTALLED to the location of your compiled git binary"
	else
		( cd "${1:-$PWD}" && git config --local --bool 'core.usewatchman' 'true' && git status >/dev/null 2>&1 )
		signal_watchman 0 || error "git isn't starting watchman"
		echo "$HOME" >"$WATCHMAN_HOME/env_home"
	fi
}

# disable watchman for the repository path specified, or the cwd
disable_watchman () {
	( cd "${1:-$PWD}" && git config --local --bool 'core.usewatchman' 'false' )
}

signal_watchman () {
	cat "$(supertrash)/.watchman_homes" | while IFS=$'\n' read dir ; do
		pkill -"$1" -f "watchman -f -U ${dir}/"
	done
}

# wait a while for watchman to exit, and return the remaining timeout;
# when 0 is returned, it means watchman did not exit in time
wait_exit_watchman () {
	local timeout=20  # 5 seconds
	while [ $timeout -gt 0 ] ; do
		sleep 0.25
		signal_watchman 0 || break
		((timeout=$timeout-1))
	done
	echo $timeout
}

stop_watchman () {
	if [ -n "$WATCHMAN_HOME" ] ; then
		signal_watchman TERM
		if [ $(wait_exit_watchman) -eq 0 ] ; then
			signal_watchman KILL
			if [ $(wait_exit_watchman) -eq 0 ] ; then
				error 'could not kill watchman'
			fi
		fi
	fi
}

cleanup_watchman () {
	if [ -n "$WATCHMAN_HOME" ] ; then
		stop_watchman
		rm -rf "$WATCHMAN_HOME"
		unset WATCHMAN_HOME
		rm -rf "$(supertrash)"
	fi
}

