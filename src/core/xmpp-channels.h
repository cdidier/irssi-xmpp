/* $Id$ */

#ifndef __XMPP_CHANNELS_H
#define __XMPP_CHANNELS_H

#include "channels.h"
#include "xmpp-servers.h"

#define XMPP_CHANNEL_SETUP(chansetup) \
	PROTO_CHECK_CAST(CHANNEL_SETUP(chansetup), CHANNEL_SETUP_REC, chat_type, "XMPP")

#define IS_XMPP_CHANNEL_SETUP(chansetup) \
	(XMPP_CHANNEL_SETUP(chansetup) ? TRUE : FALSE)

/* Returns XMPP_CHANNEL_REC if it's XMPP channel, NULL if it isn't. */
#define XMPP_CHANNEL(channel) 					\
	PROTO_CHECK_CAST(CHANNEL(channel), XMPP_CHANNEL_REC, chat_type, "XMPP")

#define IS_XMPP_CHANNEL(channel) 				\
	(XMPP_CHANNEL(channel) ? TRUE : FALSE)

#define xmpp_channel_find(server, name) 			\
	XMPP_CHANNEL(channel_find(SERVER(server), name))

typedef enum {
	XMPP_CHANNELS_FEATURE_HIDDEN                                 = 1 << 0,
	XMPP_CHANNELS_FEATURE_MEMBERS_ONLY                           = 1 << 1,
	XMPP_CHANNELS_FEATURE_MODERATED                              = 1 << 2,
	XMPP_CHANNELS_FEATURE_NONANONYMOUS                           = 1 << 3,
	XMPP_CHANNELS_FEATURE_OPEN                                   = 1 << 4,
	XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED                     = 1 << 5,
	XMPP_CHANNELS_FEATURE_PERSISTENT                             = 1 << 6,
	XMPP_CHANNELS_FEATURE_PUBLIC                                 = 1 << 7,
	XMPP_CHANNELS_FEATURE_SEMIANONYMOUS                          = 1 << 8,
	XMPP_CHANNELS_FEATURE_TEMPORARY                              = 1 << 9,
	XMPP_CHANNELS_FEATURE_UNMODERATED                            = 1 << 10,
	XMPP_CHANNELS_FEATURE_UNSECURED                              = 1 << 11
} XMPP_CHANNELS_FEATURES;

#define STRUCT_SERVER_REC XMPP_SERVER_REC
struct _XMPP_CHANNEL_REC {
	#include "channel-rec.h"

	char	*nick;
	XMPP_CHANNELS_FEATURES	features;

	time_t	 composing_time;
};

enum {
	XMPP_CHANNELS_ERROR_NONE,
	XMPP_CHANNELS_ERROR_PASSWORD_INVALID_OR_MISSING,
	XMPP_CHANNELS_ERROR_USER_BANNED,
	XMPP_CHANNELS_ERROR_ROOM_NOT_FOUND,
	XMPP_CHANNELS_ERROR_ROOM_CREATION_RESTRICTED,
	XMPP_CHANNELS_ERROR_USE_RESERVED_ROOM_NICK,
	XMPP_CHANNELS_ERROR_NOT_ON_MEMBERS_LIST,
	XMPP_CHANNELS_ERROR_NICK_IN_USE,
	XMPP_CHANNELS_ERROR_MAXIMUM_USERS_REACHED,
};

__BEGIN_DECLS
CHANNEL_REC	*xmpp_channel_create(XMPP_SERVER_REC *, const char *,
		     const char *, int, const char *);
void		 xmpp_channels_join(XMPP_SERVER_REC *, const char *, int);
void		 xmpp_channel_send_message(XMPP_SERVER_REC *, const char *,
		     const char *);

void xmpp_channels_init(void);
void xmpp_channels_deinit(void);
__END_DECLS

#endif
