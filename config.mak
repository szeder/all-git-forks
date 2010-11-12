prefix := /home/peff/local/git/$(shell git install-prefix)
CC = ccache gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -g
GIT_TEST_OPTS = --root=/dev/shm/git-tests
ASCIIDOC_NO_ROFF = nope
ASCIIDOC8 = Yes
GNU_ROFF = Yes
TEST_LINT = test-lint
GIT_PROVE_OPTS= -j16 --state=hot,all,save
DEFAULT_TEST_TARGET = prove
