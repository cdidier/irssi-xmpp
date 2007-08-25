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
	XMPP_PRESENCE_CHAT
};
extern const char *xmpp_presence_show[];
#define XMPP_PRESENCE_SHOW_LEN 7

enum {
	XMPP_SUBSCRIPTION_REMOVE,
	XMPP_SUBSCRIPTION_NONE,
	XMPP_SUBSCRIPTION_TO,
	XMPP_SUBSCRIPTION_FROM,
	XMPP_SUBSCRIPTION_BOTH
};
extern const char *xmpp_subscription[];

extern const char *xmpp_service_name;

typedef struct _XmppRosterRessource {
	char	*name;
	int	 priority;
	int	 show;
	char	*status;
} XmppRosterRessource;

typedef struct _XmppRosterUser {
	char	*jid;
	char	*name;
	int	 subscription;
	gboolean error;

	GSList	*ressources;
} XmppRosterUser;

typedef struct _XmppRosterGroup {
	char	*name;

	GSList	*users;
} XmppRosterGroup;

__BEGIN_DECLS
int		 xmpp_sort_user_func(gconstpointer, gconstpointer);
XmppRosterUser	*xmpp_find_user_from_groups(GSList *, const char *, 
		    XmppRosterGroup **);
gboolean	 xmpp_roster_show_user(XmppRosterUser *);

void	xmpp_roster_update(XMPP_SERVER_REC *, LmMessageNode *);
void	xmpp_roster_presence_update(XMPP_SERVER_REC *, const char *,
	    const char *, const char *, const char *);
void	xmpp_roster_presence_error(XMPP_SERVER_REC *, const char *);
void	xmpp_roster_presence_unavailable(XMPP_SERVER_REC *, const char *,
	    const char *);
void	xmpp_roster_cleanup(XMPP_SERVER_REC *);
__END_DECLS

#endif
