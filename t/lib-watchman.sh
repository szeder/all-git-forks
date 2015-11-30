# Shell library for testing git with watchman
#
# Copyright (c) 2015 Twitter Inc.
#

setup_watchman () {
	WATCHMAN_HOME="$(mktemp -d /tmp/watchman_home.XXXXXXXX)"
	export WATCHMAN_HOME
}

# enable watchman for the repository path specified, or the cwd
enable_watchman () {
	if [ -n "$WATCHMAN_HOME" ] ; then
		( cd "${1:-$PWD}" && git config --local --bool 'core.usewatchman' 'true' )
	else
		error "watchman is not set up for testing; you probably need to call setup_watchman ($WATCHMAN_HOME)"
	fi
}

# disable watchman for the repository path specified, or the cwd
disable_watchman () {
	( cd "${1:-$PWD}" && git config --local --bool 'core.usewatchman' 'false' )
}

signal_watchman () {
	if [ -n "$WATCHMAN_HOME" ] ; then
		pkill -"$1" -f "watchman -f -U ${WATCHMAN_HOME}/"
	fi
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
		rm -rf "$WATCHMAN_HOME"
		unset WATCHMAN_HOME
	fi
}

