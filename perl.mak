# Rules to build Git commands written in perl

ifndef GIT_ROOT_DIR
	GIT_ROOT_DIR = .
endif

ifndef PERL_PATH
	PERL_PATH = /usr/bin/perl
endif
export PERL_PATH
PERL_PATH_SQ = $(subst ','\'',$(PERL_PATH))

ifeq ($(PERL_PATH),)
NO_PERL = NoThanks
endif

ifndef NO_PERL
$(patsubst %.perl,%,$(SCRIPT_PERL)): $(GIT_ROOT_DIR)/perl/perl.mak

$(patsubst %.perl,%,$(SCRIPT_PERL)): % : %.perl $(GIT_ROOT_DIR)/GIT-VERSION-FILE
	$(QUIET_GEN)$(RM) $@ $@+ && \
	INSTLIBDIR=`MAKEFLAGS= $(MAKE) -C $(GIT_ROOT_DIR)/perl -s --no-print-directory instlibdir` && \
	sed -e '1{' \
	    -e '	s|#!.*perl|#!$(PERL_PATH_SQ)|' \
	    -e '	h' \
	    -e '	s=.*=use lib (split(/$(pathsep)/, $$ENV{GITPERLLIB} || "'"$$INSTLIBDIR"'"));=' \
	    -e '	H' \
	    -e '	x' \
	    -e '}' \
	    -e 's/@@GIT_VERSION@@/$(GIT_VERSION)/g' \
	    $@.perl >$@+ && \
	chmod +x $@+ && \
	mv $@+ $@


.PHONY: gitweb
gitweb:
	$(QUIET_SUBDIR0)gitweb $(QUIET_SUBDIR1) all

git-instaweb: git-instaweb.sh gitweb GIT-SCRIPT-DEFINES
	$(QUIET_GEN)$(cmd_munge_script) && \
	chmod +x $@+ && \
	mv $@+ $@
else # NO_PERL
$(patsubst %.perl,%,$(SCRIPT_PERL)) git-instaweb: % : unimplemented.sh
	$(QUIET_GEN)$(RM) $@ $@+ && \
	sed -e '1s|#!.*/sh|#!$(SHELL_PATH_SQ)|' \
	    -e 's|@@REASON@@|NO_PERL=$(NO_PERL)|g' \
	    unimplemented.sh >$@+ && \
	chmod +x $@+ && \
	mv $@+ $@
endif # NO_PERL
