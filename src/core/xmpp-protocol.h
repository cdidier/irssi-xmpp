#ifndef __XMPP_PROTOCOL_H
#define __XMPP_PROTOCOL_H

#define XMPP_PROTOCOL_LEVEL 1

#define XMPP_RECODE_IN 0
#define XMPP_RECODE_OUT 1

gchar  *xmpp_recode(const gchar *, const int);

gchar      *xmpp_jid_get_username(const gchar *);
gchar      *xmpp_jid_get_ressource(const gchar *);
gchar      *xmpp_jid_strip_ressource(const gchar *);
gboolean    xmpp_jid_have_ressource(const gchar *);
gboolean    xmpp_jid_have_address(const gchar *);
gboolean    xmpp_priority_out_of_bound(const int);

void    xmpp_send_message_chat(XMPP_SERVER_REC *, const gchar *, const gchar *);
void    xmpp_set_presence(XMPP_SERVER_REC *, const int, const gchar *);
void    xmpp_register_handlers(XMPP_SERVER_REC *);

#endif
