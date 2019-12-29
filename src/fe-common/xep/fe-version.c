/*
 * Copyright (C) 2007 Colin DIDIER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *	  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *			  
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */		 

#include "module.h"
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "../module-formats.h"

static void
sig_version(XMPP_SERVER_REC *server, const char *jid, const char *client,
    const char *version, const char *os)
{
	XMPP_ROSTER_USER_REC *user;
	char *name, *str;

	g_return_if_fail(jid != NULL);
	if (client == NULL && version == NULL && os == NULL)
		return;
	str = g_strconcat("is running ",
	    client != NULL ? client : "",
	    client != NULL && version != NULL ? " " : "",
	    version != NULL ? version : "",
	    (client != NULL || version != NULL) && os != NULL ? " - " : "",
	    os != NULL ? "on " : "",
	    os != NULL ? os : "", (void *)NULL);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);
	printformat_module(MODULE_NAME, server, jid, MSGLEVEL_CRAP,
	    XMPPTXT_MESSAGE_EVENT, name, str);
	g_free(name);
	g_free(str);
}

void
fe_version_init(void)
{
	signal_add("xmpp version", sig_version);
}

void
fe_version_deinit(void)
{   
	signal_remove("xmpp version", sig_version);
}
