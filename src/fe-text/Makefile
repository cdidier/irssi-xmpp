# $Id$

LIB= text_xmpp
SRCS=	text-xmpp-core.c \
	xep/text-composing.c \
	xep/text-muc.c \
	xep/text-xep.c

LIB_INCS = -I../../src/fe-text/include/irssi/src/fe-text
LIB_LIBS = `pkg-config --libs glib-2.0`

include ../rules.mk
