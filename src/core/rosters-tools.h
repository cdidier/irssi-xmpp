/* $Id$ */

#ifndef __ROSTER_TOOLS_H
#define __ROSTER_TOOLS_H

#include "rosters.h"

__BEGIN_DECLS
XMPP_ROSTER_USER_REC	 *rosters_find_user(GSList *, const char *,
			     XMPP_ROSTER_GROUP_REC **,
			     XMPP_ROSTER_RESOURCE_REC **);
XMPP_ROSTER_RESOURCE_REC *rosters_find_resource(XMPP_ROSTER_USER_REC *,
			      const char *);
XMPP_ROSTER_RESOURCE_REC *rosters_find_own_resource(XMPP_SERVER_REC *,
			      const char *);
void		 rosters_reorder(XMPP_ROSTER_GROUP_REC *);
char		*rosters_resolve_name(XMPP_SERVER_REC *, const char *);
int		 xmpp_get_show(const char *);
__END_DECLS

#endif
