# vim: syntax=make
# CFLAGS =
bindir = $(prefix)/bin
ifeq ($(uname_O),Cygwin)
	NO_MMAP = YesPlease
endif
export GIT_SKIP_TESTS = t9200
