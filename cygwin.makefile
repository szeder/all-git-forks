# build on Cygwin
SDIR := $(shell cd $(dir $(firstword $(MAKEFILE_LIST))) && pwd)
GVF := ${SDIR}/GIT-VERSION-FILE
DESTDIR := ${SDIR}/.build
prefix=/usr
libexec=${prefix}/lib
CONFIG_FLAGS=--with-libpcre
INSTALL=install

all : config make-all make-bundle

apt-cyg-deps:
	apt-cyg install -u asciidoc docbook-xml42 openssl-devel xmlto-devel w32api

config :
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

make-bundle : make-dest strip completion bzip

make-all : make make-dest

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
bzip :
	tar -C ${DESTDIR} -v -c usr etc | bzip2 > git-${VERSION}-1.tar.bz2
