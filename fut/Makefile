
default: fut

include ../Makefile

SRCS := \
    cmd_commit.c \
    cmd_help.c \
    cmd_stage.c \
    cmd_status.c \
    cmd_unstage.c \
    cmd_version.c \
    file_status.c \
    run_git.c \
    fut.c

fut: $(SRCS) ../libgit.a
	gcc $(ALL_CFLAGS) -I.. $(SRCS) -L.. $(EXTLIBS) -lgit ../xdiff/lib.a -o $@
