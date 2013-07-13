# build on Cygwin
SDIR := $(shell cd $(dir $(firstword $(MAKEFILE_LIST))) && pwd)
GVF := ${SDIR}/GIT-VERSION-FILE
DESTDIR := ${SDIR}/.build
ifeq ($(OS),Windows_NT)
	prefix=/usr
else
	# on Linux use /usr/local
	prefix=/usr/local
endif
libexec=${prefix}/lib
CONFIG_FLAGS=--with-libpcre
INSTALL=install

all : config make-all make-bundle

apt-cyg-deps:
	apt-cyg install -u asciidoc docbook-xml42 openssl-devel xmlto-devel w32api

config : make-version
	${SDIR}/configure --prefix=${prefix} --libexec=${libexec} ${CONFIG_FLAGS}

config-and-make : config make-only

config-and-install : config-and-make make-install

make-only :
	make all && make man

make :
	make all && make install && make man && make install-doc

make-dest :
	make install install-doc DESTDIR="${DESTDIR}"

make-install:
	make install install-doc

make-bundles : make-dest strip completion bzip

make-all : make make-dest

make-version :
	$(shell git describe | cut -f 1,2 -d - > version)

strip :
	find ${DESTDIR} -iname '*.exe' -print -exec strip {} \;

COMPLETION_DIR := ${DESTDIR}/etc/bash_completion.d/
completion :
	mkdir -p ${COMPLETION_DIR}
	${INSTALL} -v -m 0644 ${SDIR}/contrib/completion/{git-completion.bash,git-prompt.sh} -t ${COMPLETION_DIR}

install-completion :
	${INSTALL} -v -m 0644 ${SDIR}/contrib/completion/{git-completion.bash,git-prompt.sh} -t /etc/bash_completion.d/

# if build is successfull make tarball
ifndef VERSION
VERSION = $(shell sed -e 's/^GIT_VERSION = //; s/\.[0-9]\+\.[0-9a-z]\+$$//' ${GVF})
endif
bzip : bzip-main bzip-gui bzip-gitk bzip-completion bzip-svn

bzip-main:
	tar -C ${DESTDIR} -v -c usr etc | bzip2 > git-${VERSION}-1.tar.bz2
bzip-gui:
	gzip -c ${DESTDIR}/usr/share/man/man1/git-gui.1 > ${DESTDIR}/usr/share/man/man1/git-gui.1.gz
	tar -C ${DESTDIR} -v -c usr/{share/{git-gui,man/man1/git-gui.1.gz},lib/git-core/git-gui{,--askpass}} | bzip2 > git-gui-${VERSION}-1.tar.bz2
bzip-gitk:
	gzip -c ${DESTDIR}/usr/share/man/man1/gitk.1 > ${DESTDIR}/usr/share/man/man1/gitk.1.gz
	tar -C ${DESTDIR} -v -c usr/{bin/gitk,share/{gitk,man/man1/gitk.1.gz}} | bzip2 > gitk-${VERSION}-1.tar.bz2
bzip-completion:
	tar -C ${DESTDIR} -v -c etc/bash_completion.d | bzip2 > git-completion-${VERSION}-1.tar.bz2
bzip-svn:
	gzip -c ${DESTDIR}/usr/share/man/man1/git-svn.1 > ${DESTDIR}/usr/share/man/man1/git-svn.1.gz
	tar -C ${DESTDIR} -v -c usr/{lib/git-core/git-svn,share/man/man1/git-svn.1.gz} | bzip2 > git-svn-${VERSION}-1.tar.bz2
