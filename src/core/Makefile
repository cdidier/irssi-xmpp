# $Id$

LIB= xmpp_core
SRCS=	xmpp-commands.c \
	xmpp-core.c \
	xmpp-queries.c \
	xmpp-servers.c \
	xmpp-servers-reconnect.c \
	xmpp-session.c \
	xmpp-settings.c \
	loudmouth-tools.c \
	protocol.c \
	rosters.c \
	rosters-tools.c \
	stanzas.c \
	tools.c \
	xep/disco.c \
	xep/ping.c \
	xep/vcard.c \
	xep/version.c \
	xep/xep.c

LIB_LIBS = `pkg-config --libs loudmouth-1.0`

include ../rules.mk
