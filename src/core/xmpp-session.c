/*
 * $Id$
 *
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
#include <time.h>

#include "module.h"
#include "misc.h"
#include "settings.h"
#include "signals.h"
#include "lib-config/iconfig.h"

#include "xmpp-channels.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"
#include "xmpp-servers.h"
#include "xmpp-tools.h"

static void
sig_session_save_server(XMPP_SERVER_REC *server, CONFIG_REC *config,
    CONFIG_NODE *node)
{
	if (!IS_XMPP_SERVER(server))
		return;

	config_node_set_bool(config, node, "usermode_away", server->usermode_away);
	config_node_set_str(config, node, "away_reason", server->away_reason);

	config_node_set_int(config, node, "show", server->show);
	config_node_set_bool(config, node, "default_priority",
	    server->default_priority);
	config_node_set_int(config, node, "priority", server->priority);
}

static void
sig_session_restore_server(XMPP_SERVER_REC *server, CONFIG_NODE *node)
{
	if (!IS_XMPP_SERVER(server))
		return;

	server->usermode_away = config_node_get_bool(node, "usermode_away",
	    FALSE);
	
	signal_emit("xmpp own_presence", 4, server,
	    config_node_get_int(node, "show", XMPP_PRESENCE_AVAILABLE),
	    config_node_get_str(node, "away_reason", NULL),
	    config_node_get_bool(node, "priority", TRUE) ?
	        config_node_get_int(node, "priority", 0) : server->priority);
}

static void
sig_session_restore_nick(XMPP_CHANNEL_REC *channel, CONFIG_NODE *node)
{
}

static void
session_restore_channel(XMPP_CHANNEL_REC *channel)
{
	g_return_if_fail(IS_XMPP_CHANNEL(channel));
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	GSList *tmp;

	if (!IS_XMPP_SERVER(server) || !server->session_reconnect)
		return;

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		XMPP_CHANNEL_REC *channel = XMPP_CHANNEL(tmp->data);

		if (channel != NULL && channel->session_rejoin)
			session_restore_channel(channel);
	}
}

void
xmpp_session_init(void)
{
	signal_add("session save server",
	    (SIGNAL_FUNC)sig_session_save_server);
	signal_add("session restore server",
	    (SIGNAL_FUNC)sig_session_restore_server);
	signal_add("session restore nick",
	    (SIGNAL_FUNC)sig_session_restore_nick);
	signal_add("event connected", (SIGNAL_FUNC)sig_connected);
}

void
xmpp_session_deinit(void)
{
	signal_remove("session save server",
	    (SIGNAL_FUNC)sig_session_save_server);
	signal_remove("session restore server",
	    (SIGNAL_FUNC)sig_session_restore_server);
	signal_remove("session restore nick",
	    (SIGNAL_FUNC)sig_session_restore_nick);
	signal_remove("event connected", (SIGNAL_FUNC)sig_connected);
}
