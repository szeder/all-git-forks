Meta = $(HOME)/compile/git/Meta
prefix_base := $(shell $(Meta)/install/prefix)
ifeq ($(prefix_base), detached)
prefix := /do/not/install
else
prefix := $(HOME)/local/git/$(prefix_base)
endif

COMPILER ?= gcc
O = 0
CC = ccache $(COMPILER)
CFLAGS = -g -O$(O)
CFLAGS += -Wall -Werror
CFLAGS += -Wno-format-zero-length
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wpointer-arith
ifeq ($(COMPILER), clang)
CFLAGS += -Qunused-arguments
CFLAGS += -Wno-parentheses-equality
else
CFLAGS += -Wold-style-declaration
endif
LDFLAGS = -g

# Relax compilation on a detached HEAD (which is probably
# historical, and may contain compiler warnings that later
# got fixed).
head = $(shell git symbolic-ref HEAD 2>/dev/null)
rebasing = $(shell test -d "`git rev-parse --git-dir`/"rebase-* && echo yes)
strict_compilation = $(or $(rebasing), $(head))
ifeq ($(strict_compilation),)
  CFLAGS += -Wno-error
endif

USE_LIBPCRE = YesPlease
USE_DUMPSTAT_ZEROMQ = YesPlease

GIT_TEST_OPTS = --root=/var/ram/git-tests
TEST_LINT = test-lint
GIT_PROVE_OPTS= -j16 --state=slow,save
DEFAULT_TEST_TARGET = prove
export GIT_TEST_HTTPD = Yes
export GIT_TEST_GIT_DAEMON = Yes

GNU_ROFF = Yes
MAN_BOLD_LITERAL = Yes

-include $(Meta)/config/config.mak.local
