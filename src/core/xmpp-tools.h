/* $Id$ */

#ifndef __XMPP_TOOLS_H
#define __XMPP_TOOLS_H

__BEGIN_DECLS
char	*xmpp_recode_out(const char *);
char	*xmpp_recode_in(const char *);

char	*xmpp_jid_get_ressource(const char *);
char	*xmpp_jid_strip_ressource(const char *);
char	*xmpp_jid_get_username(const char *);
gboolean xmpp_jid_have_address(const char *);
gboolean xmpp_jid_have_ressource(const char *);
gboolean xmpp_priority_out_of_bound(const int);
__END_DECLS

#endif
