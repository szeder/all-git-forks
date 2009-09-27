DOC_MASTERDIR=..

MAN1_PO = $(wildcard git-*.po)
MAN5_PO =
MAN7_PO = gittutorial.txt gittutorial-2.txt gitglossary.txt
ARTICLES_PO =
SP_ARTICLES_PO =

MAN1_TXT=$(patsubst %.po,%.txt,$(MAN1_PO))
MAN5_TXT=$(patsubst %.po,%.txt,$(MAN5_PO))
MAN7_TXT=$(patsubst %.po,%.txt,$(MAN7_PO))
ARTICLES=$(patsubst %.po,%.txt,$(ARTICLES_PO))
SP_ARTICLES=$(patsubst %.po,%.txt,$(SP_ARTICLES_PO))

DOC_CONFIGS= asciidoc.conf user-manual.conf \
	manpage-normal.xsl manpage-base.xsl docbook.xsl docbook-xsl.css \


include ../Makefile

all: txt

txt: $(MAN_TXT)

doc.dep: txt

$(DOC_CONFIGS):	% : $(DOC_MASTERDIR)/%
	$(QUIET_LNCP)ln -s $< $@

%.txt: %.po
	$(QUIET_PO4A_TRANSLATE)if test -f "$<"; then \
	  po4a-translate -k 0 -f text -o 'asciidoc=1' -m $(DOC_MASTERDIR)/$@ -p $< -l $@ ;\
	fi

# Cannot depend on $(DOC_MASTERDIR)/%.txt to avoid autogeneration.
# If you want update *.po file. Please do:
#   $ make UPDATEPO=true foobar.po
ifdef UPDATEPO
%.po: $(DOC_MASTERDIR)/%.txt
	$(QUIET_PO4A_UPDATEPO)if test -f "$@"; then \
	  po4a-updatepo -f text -o 'asciidoc=1' --previous --msgmerge-opt '--no-location' -m $< -p $@ ;\
	fi
endif

%.pot: $(DOC_MASTERDIR)/%.txt
	$(QUIET_PO4A_GETTEXTIZE)po4a-gettextize -f text -o asciidoc=1 -m $< -p $@
	$(QUIET_PO4A_UPDATEPO)po4a-updatepo -f text -o 'asciidoc=1' --previous --msgmerge-opt '--no-location' -m $< -p $@

clean::
	rm -f *.txt *.po\~ *.pot\~
	rm -f $(DOC_CONFIGS)
