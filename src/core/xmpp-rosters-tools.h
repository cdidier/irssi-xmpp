/* $Id:*/

#ifndef __XMPP_ROSTER_TOOLS_H
#define __XMPP_ROSTER_TOOLS_H

#include "xmpp-rosters.h"

__BEGIN_DECLS
XMPP_ROSTER_USER_REC	 *xmpp_find_user(XMPP_SERVER_REC *,
			      const char *, XMPP_ROSTER_GROUP_REC **);
XMPP_ROSTER_RESOURCE_REC *xmpp_find_resource(XMPP_ROSTER_USER_REC *,
			      const char *);
char			 *xmpp_get_full_jid(XMPP_SERVER_REC *, const char *);
gboolean		  xmpp_show_user(XMPP_ROSTER_USER_REC *);
void			  xmpp_reorder_users(XMPP_ROSTER_GROUP_REC *);
__END_DECLS

#endif
