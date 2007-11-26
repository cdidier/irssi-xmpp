/* $Id$ */

#ifndef __XMPP_ROSTER_H
#define __XMPP_ROSTER_H

enum {
	XMPP_PRESENCE_ERROR,
	XMPP_PRESENCE_UNAVAILABLE,
	XMPP_PRESENCE_XA,
	XMPP_PRESENCE_DND,
	XMPP_PRESENCE_AWAY,
	XMPP_PRESENCE_AVAILABLE,
	XMPP_PRESENCE_CHAT,
	XMPP_PRESENCE_ONLINE_STR,
	XMPP_PRESENCE_SHOW_LEN
};
extern const char *xmpp_presence_show[];

enum {
	XMPP_SUBSCRIPTION_REMOVE,
	XMPP_SUBSCRIPTION_NONE,
	XMPP_SUBSCRIPTION_TO,
	XMPP_SUBSCRIPTION_FROM,
	XMPP_SUBSCRIPTION_BOTH
};
extern const char *xmpp_subscription[];

/* roster structure */
typedef struct _XMPP_ROSTER_RESOURCE_REC {
	char	*name;
	int	 priority;
	int	 show;
	char	*status;
	char	*composing_id;
} XMPP_ROSTER_RESOURCE_REC;

typedef struct _XMPP_ROSTER_USER_REC {
	char	*jid;
	char	*name;
	int	 subscription;
	gboolean error;

	GSList	*resources;
} XMPP_ROSTER_USER_REC;

typedef struct _XMPP_ROSTER_GROUP_REC {
	char	*name;

	GSList	*users;
} XMPP_ROSTER_GROUP_REC;

__BEGIN_DECLS
void xmpp_rosters_init(void);
void xmpp_rosters_deinit(void);
__END_DECLS

#endif
