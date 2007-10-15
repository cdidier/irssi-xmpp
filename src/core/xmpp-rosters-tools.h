/* $Id$ */

#ifndef __XMPP_ROSTER_TOOLS_H
#define __XMPP_ROSTER_TOOLS_H

#include "xmpp-rosters.h"

__BEGIN_DECLS
XMPP_ROSTER_USER_REC	 *xmpp_rosters_find_user(GSList *, const char *,
			     XMPP_ROSTER_GROUP_REC **);
XMPP_ROSTER_RESOURCE_REC *xmpp_rosters_find_resource(XMPP_ROSTER_USER_REC *,
			      const char *);
gboolean		  xmpp_rosters_show_user(XMPP_ROSTER_USER_REC *);
void			  xmpp_rosters_reorder(XMPP_ROSTER_GROUP_REC *);
char			 *xmpp_rosters_get_full_jid(GSList *, const char *);
int			  xmpp_presence_get_show(const char *);
__END_DECLS

#endif
