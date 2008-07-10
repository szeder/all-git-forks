# vim: syntax=make
# CFLAGS =
prefix = /usr/local
THREADED_DELTA_SEARCH = Yes
ifeq ($(uname_O),Cygwin)
	NO_MMAP = YesPlease
endif
export GIT_SKIP_TESTS = t9200
