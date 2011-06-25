all:: svn-fe$X

CC = gcc
RM = rm -f
MV = mv

CFLAGS = -g -O2 -Wall
LDFLAGS =
ALL_CFLAGS = $(CFLAGS)
ALL_LDFLAGS = $(LDFLAGS)
EXTLIBS = -lssl -lcrypto -lpcre -lz -lpthread

GIT_LIB = ../../libgit.a
VCSSVN_LIB = ../../vcs-svn/lib.a
XDIFF_LIB = ../../xdiff/lib.a
LIBS = $(VCSSVN_LIB) $(GIT_LIB) $(XDIFF_LIB) $(EXTLIBS)

QUIET_SUBDIR0 = +$(MAKE) -C # space to separate -C and subdir
QUIET_SUBDIR1 =

ifneq ($(findstring $(MAKEFLAGS),w),w)
PRINT_DIR = --no-print-directory
else # "make -w"
NO_SUBDIR = :
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
	QUIET_CC      = @echo '   ' CC $@;
	QUIET_LINK    = @echo '   ' LINK $@;
	QUIET_SUBDIR0 = +@subdir=
	QUIET_SUBDIR1 = ;$(NO_SUBDIR) echo '   ' SUBDIR $$subdir; \
	                $(MAKE) $(PRINT_DIR) -C $$subdir
endif
endif

man: svn-fe.1

svn-fe$X: svn-fe.o $(VCSSVN_LIB) $(GIT_LIB)
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ svn-fe.o \
		$(ALL_LDFLAGS) $(LIBS)

svn-fe.o: svn-fe.c ../../vcs-svn/svndump.h
	$(QUIET_CC)$(CC) -I../../vcs-svn -o $*.o -c $(ALL_CFLAGS) $<

svn-fe.html: svn-fe.txt
	$(QUIET_SUBDIR0)../../Documentation $(QUIET_SUBDIR1) \
		MAN_TXT=../contrib/svn-fe/svn-fe.txt \
		../contrib/svn-fe/$@

svn-fe.1: svn-fe.txt
	$(QUIET_SUBDIR0)../../Documentation $(QUIET_SUBDIR1) \
		MAN_TXT=../contrib/svn-fe/svn-fe.txt \
		../contrib/svn-fe/$@
	$(MV) ../../Documentation/svn-fe.1 .

$(VCSSVN_LIB) $(GIT_LIB) $(XDIFF_LIB): ../../%.a: FORCE
	$(QUIET_SUBDIR0)../.. $(QUIET_SUBDIR1) $*.a

clean:
	$(RM) svn-fe$X svn-fe.o svn-fe.html svn-fe.xml svn-fe.1

.PHONY: all man clean FORCE
