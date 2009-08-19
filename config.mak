# vim: syntax=make
# CFLAGS =
prefix = /usr/local
THREADED_DELTA_SEARCH = Yes
BLK_SHA1 = Yes
ifeq ($(uname_O),Cygwin)
	NO_MMAP = YesPlease
endif
export GIT_SKIP_TESTS = t9200
