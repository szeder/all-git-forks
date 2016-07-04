#
# Copyright (C) 2013
#     Matthieu Moy <Matthieu.Moy@imag.fr>
#
# To build and test:
#
#   make
#   bin-wrapper/git mw preview Some_page.mw
#   bin-wrapper/git clone mediawiki::http://example.com/wiki/
#
# To install, run Git's toplevel 'make install' then run:
#
#   make install

prefix ?= /usr/local
mandir ?= $(prefix)/share/man
man1dir ?= $(mandir)/man1
htmldir ?= $(prefix)/share/doc/git-doc

GIT_MEDIAWIKI_PM=Git/Mediawiki.pm
SCRIPT_PERL=git-remote-mediawiki.perl
SCRIPT_PERL+=git-mw.perl
GIT_ROOT_DIR=../..
HERE=contrib/mw-to-git/

INSTALL  ?= install
RM       ?= rm -f

ASCIIDOC = asciidoc
XMLTO    = xmlto

ASCIIDOC_CONF = ../../Documentation/asciidoc.conf
MANPAGE_XSL   = ../../Documentation/manpage-normal.xsl

SCRIPT_PERL_FULL=$(patsubst %,$(HERE)/%,$(SCRIPT_PERL))
INSTLIBDIR=$(shell $(MAKE) -C $(GIT_ROOT_DIR)/perl \
                -s --no-print-directory instlibdir)
DESTDIR_SQ = $(subst ','\'',$(DESTDIR))
INSTLIBDIR_SQ = $(subst ','\'',$(INSTLIBDIR))

GIT_MEDIAWIKI_DOC := git-mediawiki.1
GIT_MEDIAWIKI_XML := git-mediawiki.xml
GIT_MEDIAWIKI_TXT := git-mediawiki.txt
GIT_MEDIAWIKI_HTML := git-mediawiki.html

all: build

doc: $(GIT_MEDIAWIKI_DOC) $(GIT_MEDIAWIKI_HTML)

install-doc: install-man install-html

install-man: $(GIT_MEDIAWIKI_DOC)
	$(INSTALL) -d -m 755 $(DESTDIR)$(man1dir)
	$(INSTALL) -m 644 $^ $(DESTDIR)$(man1dir)

install-html: $(GIT_MEDIAWIKI_HTML)
	$(INSTALL) -d -m 755 $(DESTDIR)$(htmldir)
	$(INSTALL) -m 644 $^ $(DESTDIR)$(htmldir)

$(GIT_MEDIAWIKI_DOC): $(GIT_MEDIAWIKI_XML)
	$(XMLTO) -m $(MANPAGE_XSL) man $^

$(GIT_MEDIAWIKI_XML): $(GIT_MEDIAWIKI_TXT)
	$(ASCIIDOC) -b docbook -d manpage -f $(ASCIIDOC_CONF) \
		-agit_version=$(GIT_VERSION) $^

$(GIT_MEDIAWIKI_HTML): $(GIT_MEDIAWIKI_TXT)
	$(ASCIIDOC) -b xhtml11 -d manpage -f $(ASCIIDOC_CONF) \
		-agit_version=$(GIT_VERSION) $^

test: all
	$(MAKE) -C t

check: perlcritic test

install_pm:
	$(INSTALL) -d -m 755 '$(DESTDIR_SQ)$(INSTLIBDIR_SQ)/Git'
	$(INSTALL) -m 644 $(GIT_MEDIAWIKI_PM) \
		'$(DESTDIR_SQ)$(INSTLIBDIR_SQ)/$(GIT_MEDIAWIKI_PM)'

build:
	$(MAKE) -C $(GIT_ROOT_DIR) SCRIPT_PERL="$(SCRIPT_PERL_FULL)" \
                build-perl-script

install: install_pm
	$(MAKE) -C $(GIT_ROOT_DIR) SCRIPT_PERL="$(SCRIPT_PERL_FULL)" \
                install-perl-script

clean:
	$(MAKE) -C $(GIT_ROOT_DIR) SCRIPT_PERL="$(SCRIPT_PERL_FULL)" \
                clean-perl-script

perlcritic:
	perlcritic -5 $(SCRIPT_PERL)
	-perlcritic -2 $(SCRIPT_PERL)

.PHONY: all doc test check install_pm install install-doc install-man install-html clean perlcritic
