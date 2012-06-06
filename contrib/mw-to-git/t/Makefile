# Run tests
#
# Copyright (c) 2005 Junio C Hamano
#

-include ../../../config.mak.autogen
-include ../../../config.mak

ifndef PERL_PATH
	PERL_PATH = /usr/bin/perl
endif
ifndef gitexecdir
	gitexecdir = $(shell git --exec-path)
endif

PERL_PATH_SQ = $(subst ','\'',$(PERL_PATH))
gitexecdir_SQ = $(subst ','\'',$(gitexecdir))
T = $(wildcard t[0-9][0-9][0-9][0-9]-*.sh)

.PHONY: help test clean

help:
	@echo 'This is the help target of the Makefile. Current configuration:'
	@echo '  gitexecdir = $(gitexecdir_SQ)'
	@echo '  PERL_PATH = $(PERL_PATH_SQ)'
	@echo 'Run "$(MAKE) test" to launch test scripts'

test:
	for t in $(T); do "./$$t"; done

clean:
	$(RM) -r 'trash directory'.*