Meta = $(HOME)/compile/git/Meta
prefix := $(HOME)/local/git/$(shell $(Meta)/install/prefix)

CC = ccache gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -g

# Relax compilation on a detached HEAD (which is probably
# historical, and may contain compiler warnings that later
# got fixed).
ifeq ($(shell git symbolic-ref HEAD 2>/dev/null),)
  CFLAGS += -Wno-error=unused-but-set-variable
endif

USE_LIBPCRE = YesPlease

GIT_TEST_OPTS = --root=/run/shm/git-tests
TEST_LINT = test-lint
GIT_PROVE_OPTS= -j16 --state=hot,all,save
DEFAULT_TEST_TARGET = prove
export GIT_TEST_HTTPD = Yes

GNU_ROFF = Yes
MAN_BOLD_LITERAL = Yes
