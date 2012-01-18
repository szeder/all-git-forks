#!/bin/sh
#
# Copyright (c) 2010 Ævar Arnfjörð Bjarmason
#
# This is Git's interface to gettext.sh. See po/README for usage
# instructions.

# Export the TEXTDOMAIN* data that we need for Git
TEXTDOMAIN=git
export TEXTDOMAIN
if test -z "$GIT_TEXTDOMAINDIR"
then
	TEXTDOMAINDIR="@@LOCALEDIR@@"
else
	TEXTDOMAINDIR="$GIT_TEXTDOMAINDIR"
fi
export TEXTDOMAINDIR

# First decide what scheme to use...
GIT_INTERNAL_GETTEXT_SH_SCHEME=fallthrough
if test -n "@@NO_GETTEXT@@$GIT_INTERNAL_GETTEXT_TEST_FALLBACKS"
then
	: no probing necessary
elif test -n "$GIT_GETTEXT_POISON"
then
	GIT_INTERNAL_GETTEXT_SH_SCHEME=poison
elif type gettext.sh >/dev/null 2>&1
then
	# This is GNU libintl's gettext.sh, we don't need to do anything
	# else than setting up the environment and loading gettext.sh
	GIT_INTERNAL_GETTEXT_SH_SCHEME=gnu
elif test "$(gettext -h 2>&1)" = "-h"
then
	# We don't have gettext.sh, but there's a gettext binary in our
	# path. This is probably Solaris or something like it which has a
	# gettext implementation that isn't GNU libintl.
	GIT_INTERNAL_GETTEXT_SH_SCHEME=solaris
fi
export GIT_INTERNAL_GETTEXT_SH_SCHEME

# ... and then follow that decision
case "$GIT_INTERNAL_GETTEXT_SH_SCHEME" in
gnu)
	# Try to use libintl's gettext.sh, or fall back to English if we
	# can't.
	. gettext.sh
	;;
solaris)
	# Solaris has a gettext(1) but no eval_gettext(1)
	eval_gettext () {
		gettext "$1" | (
			export PATH $(git sh-i18n--envsubst --variables "$1");
			git sh-i18n--envsubst "$1"
		)
	}
	;;
poison)
	# Emit garbage so that tests that incorrectly rely on translatable
	# strings will fail.
	gettext () {
		printf "%s" "# GETTEXT POISON #"
	}

	eval_gettext () {
		printf "%s" "# GETTEXT POISON #"
	}
	;;
*)
	gettext () {
		printf "%s" "$1"
	}

	eval_gettext () {
		printf "%s" "$1" | (
			export PATH $(git sh-i18n--envsubst --variables "$1");
			git sh-i18n--envsubst "$1"
		)
	}
	;;
esac

# Git-specific wrapper functions
gettextln () {
	gettext "$1"
	echo
}

eval_gettextln () {
	eval_gettext "$1"
	echo
}
