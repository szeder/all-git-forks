prefix := /home/peff/local/git/$(shell git install-prefix)
CC = ccache gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -g
GIT_TEST_OPTS = --root=/dev/shm/git-tests
GNU_ROFF = Yes
MAN_BOLD_LITERAL = Yes
TEST_LINT = test-lint
GIT_PROVE_OPTS= -j16 --state=hot,all,save
DEFAULT_TEST_TARGET = prove
