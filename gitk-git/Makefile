# The default target of this Makefile is...
all::

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

prefix ?= $(HOME)
bindir ?= $(prefix)/bin
sharedir ?= $(prefix)/share
gitk_libdir   ?= $(sharedir)/gitk/lib
msgsdir    ?= $(gitk_libdir)/msgs
msgsdir_SQ  = $(subst ','\'',$(msgsdir))

TCL_PATH ?= tclsh
TCLTK_PATH ?= wish
INSTALL ?= install
RM ?= rm -f

DESTDIR_SQ = $(subst ','\'',$(DESTDIR))
bindir_SQ = $(subst ','\'',$(bindir))
libdir_SQ = $(subst ','\'',$(gitk_libdir))
TCLTK_PATH_SQ = $(subst ','\'',$(TCLTK_PATH))

## po-file creation rules
XGETTEXT   ?= xgettext
ifdef NO_MSGFMT
	MSGFMT ?= $(TCL_PATH) po/po2msg.sh
else
	MSGFMT ?= msgfmt
	ifneq ($(shell $(MSGFMT) --tcl -l C -d . /dev/null 2>/dev/null; echo $$?),0)
		MSGFMT := $(TCL_PATH) po/po2msg.sh
	endif
endif

PO_TEMPLATE = po/gitk.pot
ALL_POFILES = $(wildcard po/*.po)
ALL_MSGFILES = $(subst .po,.msg,$(ALL_POFILES))

ifndef V
	QUIET          = @
	QUIET_GEN      = $(QUIET)echo '   ' GEN $@ &&
endif

all:: gitk-wish $(ALL_MSGFILES)

GITK_MACOSXAPP :=

ifeq ($(uname_S),Darwin)
	GITK_MACOSXAPP := YesPlease
endif

ifdef GITK_MACOSXAPP
all:: Gitk.app

TKFRAMEWORK = /System/Library/Frameworks/Tk.framework/Resources/Wish Shell.app
TKEXECUTABLE = Wish Shell
GIT_VERSION = 1.7.3.2

Gitk.app: macosx/Info.plist macosx/gitk.icns macosx/AppMain.tcl macosx/activate_by_pid
	$(QUIET_GEN)rm -rf '$@' '$@'+ && \
	mkdir -p '$@'+/Contents/MacOS && \
	mkdir -p '$@'+/Contents/Resources/Scripts && \
	ln -s '$(subst ','\'',$(subst \,,$(TKFRAMEWORK)/Contents/MacOS/$(TKEXECUTABLE)))' \
		'$@'+/Contents/MacOS/Wish && \
	cp macosx/gitk.icns '$@'+/Contents/Resources && \
	cp macosx/activate_by_pid '$@'+/Contents/Resources && \
	sed -e 's/@@GIT_VERSION@@/$(GIT_VERSION)/g' \
		macosx/Info.plist \
		>'$@'+/Contents/Info.plist && \
	sed -e 's|@@gitexecdir@@|$(gitexecdir_SQ)|' \
		-e 's|@@GITGUI_LIBDIR@@|$(libdir_SED)|' \
		macosx/AppMain.tcl \
		>'$@'+/Contents/Resources/Scripts/AppMain.tcl && \
	mv '$@'+ '$@'

CFLAGS ?= -O2 -Wall

macosx/activate_by_pid: macosx/activate_by_pid.c
	$(CC) $(CFLAGS) $< -o $@ -framework Carbon
endif

install:: all
	$(INSTALL) -m 755 gitk-wish '$(DESTDIR_SQ)$(bindir_SQ)'/gitk
ifdef GITK_MACOSXAPP
	find 'Gitk.app' | cpio -pud '$(DESTDIR_SQ)$(libdir_SQ)'
endif
	$(INSTALL) -d -m 755 '$(DESTDIR_SQ)$(msgsdir_SQ)'
	$(foreach p,$(ALL_MSGFILES), $(INSTALL) -m 644 $p '$(DESTDIR_SQ)$(msgsdir_SQ)' &&) true

uninstall::
	$(foreach p,$(ALL_MSGFILES), $(RM) '$(DESTDIR_SQ)$(msgsdir_SQ)'/$(notdir $p) &&) true
	$(RM) '$(DESTDIR_SQ)$(bindir_SQ)'/gitk

clean::
	$(RM) gitk-wish po/*.msg

gitk-wish: gitk
	$(QUIET_GEN)$(RM) $@ $@+ && \
	sed -e '1,3s|^exec .* "$$0"|exec $(subst |,'\|',$(TCLTK_PATH_SQ)) "$$0"|' <gitk >$@+ && \
	chmod +x $@+ && \
	mv -f $@+ $@

$(PO_TEMPLATE): gitk
	$(XGETTEXT) -kmc -LTcl -o $@ gitk
update-po:: $(PO_TEMPLATE)
	$(foreach p, $(ALL_POFILES), echo Updating $p ; msgmerge -U $p $(PO_TEMPLATE) ; )
$(ALL_MSGFILES): %.msg : %.po
	@echo Generating catalog $@
	$(MSGFMT) --statistics --tcl $< -l $(basename $(notdir $<)) -d $(dir $@)

