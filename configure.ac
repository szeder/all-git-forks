#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([git], [1.4.0], [git@vger.kernel.org])

AC_CONFIG_SRCDIR([git.c])

# Definitions of macros
# MY_APPEND_LINE(LINE)
# ------------------------------
# Append LINE to file config.mak.append
AC_DEFUN([MY_APPEND_LINE],
[[echo "$1" >> config.mak.append]])# AC_APPEND_LINE

# Checks for libraries.
AC_MSG_NOTICE(CHECKS for libraries)
AC_CHECK_LIB([crypto], [SHA1_Init],,MY_APPEND_LINE(NO_OPENSSL=YesPlease))
AC_CHECK_LIB([curl], [curl_easy_setopt],,MY_APPEND_LINE(NO_CURL=YesPlease))
AC_CHECK_LIB([expat], [XML_ParserCreate],,MY_APPEND_LINE(NO_EXPAT=YesPlease))

# Output files
AC_CONFIG_FILES([config.mak:config.mak.in:config.mak.append], 
[rm -f config.mak.append], 
[echo >> config.mak.append])
AC_OUTPUT
