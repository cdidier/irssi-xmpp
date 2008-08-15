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

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"

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
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server) || !server->connrec->reconnection)
		return;

	signal_emit("xmpp own_presence", 4, server, server->connrec->show,
	    server->connrec->away_reason, server->connrec->priority);
	g_free_and_null(server->connrec->away_reason);
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
