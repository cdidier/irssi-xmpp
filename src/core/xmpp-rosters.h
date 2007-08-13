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
extern const gchar *xmpp_presence_show[];

enum {
    XMPP_SUBSCRIPTION_REMOVE,
    XMPP_SUBSCRIPTION_NONE,
    XMPP_SUBSCRIPTION_TO,
    XMPP_SUBSCRIPTION_FROM,
    XMPP_SUBSCRIPTION_BOTH
};
extern const gchar *xmpp_subscription[];

extern const gchar *xmpp_service_name;

typedef struct _XmppRosterRessource {
    gchar  *name;
    gint    priority;
    gint    show;
    gchar  *status;
} XmppRosterRessource;

typedef struct _XmppRosterUser {
    gchar  *jid;
    gchar  *name;
    gint    subscription;
    gboolean error;

    GSList *ressources;
} XmppRosterUser;

typedef struct _XmppRosterGroup {
    gchar  *name;

    GSList *users;
} XmppRosterGroup;

gint    xmpp_sort_user_func(gconstpointer, gconstpointer);
XmppRosterUser *xmpp_find_user_from_groups(GSList *, const gchar *, 
    XmppRosterGroup **);
gboolean        xmpp_roster_show_user(XmppRosterUser *);
void    xmpp_roster_update(XMPP_SERVER_REC *, LmMessageNode *);
void    xmpp_roster_update_presence(XMPP_SERVER_REC *, const gchar *,
    const gchar *, const gchar *, const gchar *);
void    xmpp_roster_presence_error(XMPP_SERVER_REC *, const gchar *);
void    xmpp_roster_presence_unavailable(XMPP_SERVER_REC *, const gchar *,
    const gchar *);
void    xmpp_roster_cleanup(XMPP_SERVER_REC *);

#endif
