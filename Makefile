# $Id$

include config.mk

all clean user-install:
	@cd src/ && ${MAKE} $@

install:
	@cd src/ && ${MAKE} $@
	@echo installing documentation files to ${DESTDIR}${IRSSI_DOC}/irssi-xmpp
	@echo installing command help files to ${DESTDIR}${IRSSI_HELP}

.PHONY: all clean user-install install
