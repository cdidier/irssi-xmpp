LIB= xmpp_core
SRCS=	xmpp-commands.c \
	xmpp-core.c \
	xmpp-queries.c \
	xmpp-servers.c \
	xmpp-servers-reconnect.c \
	xmpp-settings.c \
	loudmouth-tools.c \
	protocol.c \
	rosters.c \
	rosters-tools.c \
	stanzas.c \
	tools.c \
	xep/chatstates.c \
	xep/composing.c \
	xep/datetime.c \
	xep/delay.c \
	xep/disco.c \
	xep/muc-affiliation.c \
	xep/muc-commands.c \
	xep/muc-events.c \
	xep/muc-nicklist.c \
	xep/muc-reconnect.c \
	xep/muc-role.c \
	xep/muc.c \
	xep/oob.c \
	xep/ping.c \
	xep/registration.c \
	xep/tool_datalist.c \
	xep/vcard.c \
	xep/version.c \
	xep/xep.c

LIB_LIBS = `pkg-config --libs loudmouth-1.0 | sed -e's/-lidn//'`

include ../rules.mk
