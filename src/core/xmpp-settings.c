/*
 * $Id$
 *
 * Copyright (C) 2007 Colin DIDIER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <string.h>

#include "module.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "xmpp-rosters.h"
#include "xmpp-protocol.h"

void
read_settings(void)
{
	GSList *tmp;
	XMPP_SERVER_REC *server;
	const char *str;
	
	for (tmp = servers; tmp != NULL; tmp = tmp->next) {

		server = XMPP_SERVER(tmp->data);
		if (server == NULL)
			continue;

		/* update priority */
		if (server->priority != settings_get_int("xmpp_priority")) {
			signal_emit("xmpp own_presence", 4, server,
			    server->show, server->away_reason,
			    settings_get_int("xmpp_priority"));
		}

		/* update nick */
		if (settings_get_bool("xmpp_set_nick_as_username")) {
			if (strcmp(server->nickname, server->user) != 0) {
				g_free(server->nick);
				g_free(server->nickname);
				server->nick = g_strdup(server->user);
				server->nickname = g_strdup(server->user);
			}
		} else {
			if (strcmp(server->nickname, server->jid) != 0) {
				g_free(server->nick);
				g_free(server->nickname);
				server->nick = g_strdup(server->jid);
				server->nickname = g_strdup(server->jid);
			}
		}
	}

	/* check validity */

	str = settings_get_str("xmpp_proxy_type");
	if (settings_get_bool("xmpp_use_proxy")
	    && (str == NULL || g_ascii_strcasecmp(str, XMPP_PROXY_HTTP) != 0))
		;

	str = settings_get_str("xmpp_default_away_mode");
	if (str == NULL
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_AWAY]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_CHAT]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_DND]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_XA]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_ONLINE_STR]) != 0)
		;
}

void
xmpp_settings_init(void)
{
	signal_add("setup changed", (SIGNAL_FUNC)read_settings);
}

void
xmpp_settings_deinit(void)
{
	signal_remove("setup changed", (SIGNAL_FUNC)read_settings);
}
