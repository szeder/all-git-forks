MAIN:=git-credential-libsecret
all:: $(MAIN)

CC = gcc
RM = rm -f
CFLAGS = -g -O2 -Wall

-include ../../../config.mak.autogen
-include ../../../config.mak

INCS:=$(shell pkg-config --cflags glib-2.0 libsecret-1 gcr-base-3)
LIBS:=$(shell pkg-config --libs glib-2.0 libsecret-1 gcr-base-3)

SRCS:=$(MAIN).c
OBJS:=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(INCS) -o $@ -c $<

$(MAIN): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	@$(RM) $(MAIN) $(OBJS)
