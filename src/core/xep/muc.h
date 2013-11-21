#ifndef __MUC_H
#define __MUC_H

#include "channels.h"
#include "channels-setup.h"
#include "xmpp-servers.h"
#include "tools.h"

#define XMLNS_MUC	"http://jabber.org/protocol/muc"
#define XMLNS_MUC_USER	"http://jabber.org/protocol/muc#user"
#define XMLNS_MUC_OWNER	"http://jabber.org/protocol/muc#owner"

#define muc_extract_nick(jid)						\
	xmpp_extract_resource(jid)
#define muc_extract_channel(jid)					\
	xmpp_strip_resource(jid)

#define MUC_SETUP(chansetup) \
	PROTO_CHECK_CAST(CHANNEL_SETUP(chansetup), CHANNEL_SETUP_REC, chat_type, "XMPP")

#define IS_MUC_SETUP(chansetup) \
	(MUC_SETUP(chansetup) ? TRUE : FALSE)

/* Returns MUC_REC if it's XMPP channel, NULL if it isn't. */
#define MUC(channel) 							\
	PROTO_CHECK_CAST(CHANNEL(channel), MUC_REC, chat_type, "XMPP")

#define IS_MUC(channel) 						\
	(MUC(channel) ? TRUE : FALSE)

#define muc_find(server, name) 						\
	MUC(channel_find(SERVER(server), name))

#define STRUCT_SERVER_REC XMPP_SERVER_REC
struct _MUC_REC {
	#include "channel-rec.h"

	char	*nick;
};

enum {
	MUC_ERROR_UNKNOWN,
	MUC_ERROR_PASSWORD_INVALID_OR_MISSING	= 401,
	MUC_ERROR_USER_BANNED			= 403,
	MUC_ERROR_ROOM_NOT_FOUND		= 404,
	MUC_ERROR_ROOM_CREATION_RESTRICTED	= 405,
	MUC_ERROR_USE_RESERVED_ROOM_NICK	= 406,
	MUC_ERROR_NOT_ON_MEMBERS_LIST		= 407,
	MUC_ERROR_NICK_IN_USE			= 409,
	MUC_ERROR_MAXIMUM_USERS_REACHED		= 503,
};

__BEGIN_DECLS
void muc_join(XMPP_SERVER_REC *, const char *, gboolean);
void muc_part(MUC_REC *, const char *);
void muc_nick(MUC_REC *, const char *);
MUC_REC	*get_muc(XMPP_SERVER_REC *, const char *);

void muc_init(void);
void muc_deinit(void);
__END_DECLS

#endif
