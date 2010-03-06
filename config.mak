# vim: syntax=make
# CFLAGS =
prefix = /usr/local
CFLAGS = -O2 -Wall -Wdeclaration-after-statement -g
THREADED_DELTA_SEARCH = Yes
BLK_SHA1 = Yes
COMPUTE_HEADER_DEPENDENCIES = Yes
ifeq ($(uname_O),Cygwin)
	NO_MMAP = YesPlease
endif
export GIT_SKIP_TESTS = t9200
