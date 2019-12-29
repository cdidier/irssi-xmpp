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

#include <string.h>

#include "module.h"
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "rosters.h"

void
read_settings(void)
{
	GSList *tmp;
	XMPP_SERVER_REC *server;
	
	for (tmp = servers; tmp != NULL; tmp = tmp->next) {
		if ((server = XMPP_SERVER(tmp->data)) == NULL)
			continue;
		/* update priority */
		if (server->show == XMPP_PRESENCE_AWAY) {
			if (server->priority != settings_get_int("xmpp_priority_away"))
				signal_emit("xmpp set presence", 4, server,
				    server->show, server->away_reason,
				    settings_get_int("xmpp_priority_away"));
		} else {
			if (server->priority != settings_get_int("xmpp_priority"))
				signal_emit("xmpp set presence", 4, server,
				    server->show, server->away_reason,
				    settings_get_int("xmpp_priority"));
		}
		/* update nick */
		if (settings_get_bool("xmpp_set_nick_as_username")) {
			if (strcmp(server->nick, server->user) != 0) {
				g_free(server->nick);
				server->nick = g_strdup(server->user);
			}
		} else {
			if (strcmp(server->nick, server->jid) != 0) {
				g_free(server->nick);
				server->nick = g_strdup(server->jid);
			}
		}
	}

#if 0
	const char *str;

	/* check validity */
	str = settings_get_str("xmpp_proxy_type");
	/* TODO print error message */
	if (settings_get_bool("xmpp_use_proxy")
	    && (str == NULL || g_ascii_strcasecmp(str, XMPP_PROXY_HTTP) != 0))
		;
	str = settings_get_str("xmpp_default_away_mode");
	if (str == NULL
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_AWAY]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_CHAT]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_DND]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_XA]) != 0
	    || g_ascii_strcasecmp(str, xmpp_presence_show[XMPP_PRESENCE_ONLINE]) != 0)
		;
#endif
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
