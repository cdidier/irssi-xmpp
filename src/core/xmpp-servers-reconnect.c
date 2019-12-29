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
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"

static void
sig_server_connect_copy(SERVER_CONNECT_REC **dest, XMPP_SERVER_CONNECT_REC *src)
{
	XMPP_SERVER_CONNECT_REC *conn;

	g_return_if_fail(dest != NULL);
	if (!IS_XMPP_SERVER_CONNECT(src))
		return;
	conn = g_new0(XMPP_SERVER_CONNECT_REC, 1);
	conn->chat_type = XMPP_PROTOCOL;
	conn->show = src->show;
	conn->priority = src->priority;
	conn->prompted_password = g_strdup(src->prompted_password);
	g_free(src->nick);
	src->nick = src->real_jid;
	src->real_jid = NULL;
	*dest = (SERVER_CONNECT_REC *)conn;
}

static void
sig_save_status(XMPP_SERVER_CONNECT_REC *conn, XMPP_SERVER_REC *server)
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
	signal_emit("xmpp set presence", 4, server, server->connrec->show,
	    server->connrec->away_reason, server->connrec->priority);
	g_free_and_null(server->connrec->away_reason);
}

void
xmpp_servers_reconnect_init(void)
{
	signal_add_first("server connect copy", sig_server_connect_copy);
	signal_add("server reconnect save status", sig_save_status);
	signal_add_last("server connected", sig_connected);
}

void
xmpp_servers_reconnect_deinit(void)
{
	signal_remove("server connect copy", sig_server_connect_copy);
	signal_remove("server reconnect save status", sig_save_status);
	signal_remove("server connected", sig_connected);
}
