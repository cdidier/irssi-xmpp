LIB= fe_xmpp
SRCS=	fe-xmpp-messages.c \
	fe-xmpp-queries.c \
	fe-xmpp-status.c \
	fe-xmpp-windows.c \
	fe-rosters.c \
	fe-stanzas.c \
	fe-xmpp-core.c \
	module-formats.c \
	xmpp-completion.c \
	xmpp-formats.c \
	xep/fe-carbons.c \
	xep/fe-composing.c \
	xep/fe-delay.c \
	xep/fe-muc.c \
	xep/fe-ping.c \
	xep/fe-registration.c \
	xep/fe-vcard.c \
	xep/fe-version.c \
	xep/fe-xep.c


LIB_INCS = -I../../src/fe-common 
LIB_LIBS = `pkg-config --libs glib-2.0`

include ../rules.mk
