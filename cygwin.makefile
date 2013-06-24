# build on Cygwin
SDIR := $(shell cd $(dir $(firstword $(MAKEFILE_LIST))) && pwd)
GVF := ${SDIR}/GIT-VERSION-FILE
DESTDIR := ${SDIR}/.build
prefix=/usr
libexec=${prefix}/lib
CONFIG_FLAGS=--with-libpcre

all : config make-all strip completion bzip

config :
	${SDIR}/configure --prefix=${prefix} --libexec=${libexec} ${CONFIG_FLAGS}

make :
	make all && make install && make man && make install-doc

make-dest :
	make install install-doc DESTDIR="${DESTDIR}"

make-all : make make-dest

strip :
	find ${DESTDIR} -iname '*.exe' -print -exec strip {} \;

COMPLETION_DIR := ${DESTDIR}/etc/bash_completion.d/
completion :
	mkdir -p ${COMPLETION_DIR}
	cp ${SDIR}/contrib/completion/{git-completion.bash,git-prompt.sh} ${COMPLETION_DIR}

# if build is successfull make tarball
ifndef VERSION
VERSION = $(shell sed -e 's/^GIT_VERSION = //; s/\.[0-9]\+\.[0-9a-z]\+$$//' ${GVF})
endif
bzip :
	tar -C ${DESTDIR} -v -c usr etc | bzip2 > git-${VERSION}-1.tar.bz2
