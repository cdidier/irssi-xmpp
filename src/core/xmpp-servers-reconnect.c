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

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"

static void
save_channels(XMPP_SERVER_REC *server, XMPP_SERVER_CONNECT_REC *conn)
{
	GSList *tmp;
	XMPP_CHANNEL_REC *channel;
	char *str;

	if (!IS_XMPP_SERVER(server) || !IS_XMPP_SERVER_CONNECT(conn))
		return;

	if (conn->channels_list != NULL) {
		for (tmp = conn->channels_list; tmp != NULL; tmp = tmp->next)
			g_free(tmp->data);
		g_slist_free(conn->channels_list);
		conn->channels_list = NULL;
	}

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;

		if (channel->key != NULL)
			str = g_strdup_printf("\"%s/%s\" \"%s\"",
			    channel->name, channel->nick, channel->key);
		else
			str = g_strdup_printf("\"%s/%s\"", channel->name,
			    channel->nick);

		conn->channels_list = g_slist_append(conn->channels_list,
		    g_strdup(str));
	}
}

static void
restore_channels(XMPP_SERVER_REC *server)
{
	GSList *tmp;

	if (!IS_XMPP_SERVER(server) && server->connrec->channels_list != NULL)
		return;

	for (tmp = server->connrec->channels_list; tmp != NULL;
	    tmp = tmp->next) {
		xmpp_channels_join_automatic(server, tmp->data);
		g_free(tmp->data);
	}
	g_slist_free(server->connrec->channels_list);
	server->connrec->channels_list = NULL;
}

static void
sig_server_connect_copy(SERVER_CONNECT_REC **dest, XMPP_SERVER_CONNECT_REC *src)
{
	XMPP_SERVER_CONNECT_REC *rec;

	g_return_if_fail(dest != NULL);
	if (!IS_XMPP_SERVER_CONNECT(src))
		return;

	rec = g_new0(XMPP_SERVER_CONNECT_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;
	*dest = (SERVER_CONNECT_REC *)rec;
}

static void
sig_server_reconnect_save_status(XMPP_SERVER_CONNECT_REC *conn,
    XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER_CONNECT(conn) || !IS_XMPP_SERVER(server)
	    || !server->connected)
		return;

	conn->show = server->show;
	conn->priority = server->priority;
	conn->default_priority = server->default_priority;

	save_channels(server, conn);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server) || !server->connrec->reconnection)
		return;

	signal_emit("xmpp own_presence", 4, server, server->connrec->show,
	    server->connrec->away_reason, server->connrec->priority);
	g_free_and_null(server->connrec->away_reason);

	restore_channels(server);
}

void
xmpp_servers_reconnect_init(void)
{
	signal_add("server connect copy",
	    (SIGNAL_FUNC)sig_server_connect_copy);
	signal_add("server reconnect save status",
	    (SIGNAL_FUNC)sig_server_reconnect_save_status);
	signal_add("event connected", (SIGNAL_FUNC)sig_connected);
}

void
xmpp_servers_reconnect_deinit(void)
{
	signal_remove("server connect copy",
	     (SIGNAL_FUNC)sig_server_connect_copy);
	signal_remove("server reconnect save status",
	     (SIGNAL_FUNC)sig_server_reconnect_save_status);
	signal_remove("event connected", (SIGNAL_FUNC)sig_connected);
}
