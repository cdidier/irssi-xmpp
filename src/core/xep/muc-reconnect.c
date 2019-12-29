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
#include <irssi/src/core/servers-reconnect.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "muc.h"

static void
sig_conn_copy(SERVER_CONNECT_REC **dest, XMPP_SERVER_CONNECT_REC *src)
{
	GSList *tmp;
	XMPP_SERVER_CONNECT_REC *conn;

	g_return_if_fail(dest != NULL);
	if (!IS_XMPP_SERVER_CONNECT(src))
		return;
	conn = (XMPP_SERVER_CONNECT_REC *)*dest;
	conn->channels_list = NULL;
	for (tmp = src->channels_list; tmp != NULL; tmp = tmp->next) {
		conn->channels_list = g_slist_append(conn->channels_list,
		    g_strdup(tmp->data));
	}
}

static void
sig_conn_remove(RECONNECT_REC *rec)
{
	XMPP_SERVER_CONNECT_REC *conn;

	if (!IS_XMPP_SERVER_CONNECT(rec->conn))
		return;
	conn = XMPP_SERVER_CONNECT(rec->conn);
	if (conn->channels_list != NULL) {
		g_slist_foreach(conn->channels_list, (GFunc)g_free, NULL);
		g_slist_free(conn->channels_list);
		conn->channels_list = NULL;
	}
}

static void
save_channels(XMPP_SERVER_REC *server, XMPP_SERVER_CONNECT_REC *conn)
{
	GSList *tmp;
	MUC_REC *channel;
	char *joindata;

	if (conn->channels_list != NULL) {
		g_slist_foreach(conn->channels_list, (GFunc)g_free, NULL);
		g_slist_free(conn->channels_list);
		conn->channels_list = NULL;
	}
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;
		joindata = channel->get_join_data(CHANNEL(channel));
		conn->channels_list = g_slist_append(conn->channels_list,
		    joindata);
	}
}

static void
restore_channels(XMPP_SERVER_REC *server)
{
	GSList *tmp;

	if (server->connrec->channels_list != NULL)
		return;
	for (tmp = server->connrec->channels_list; tmp != NULL;
	    tmp = tmp->next) {
	    	muc_join(server, tmp->data, TRUE);
		g_free(tmp->data);
	}
	g_slist_free(server->connrec->channels_list);
	server->connrec->channels_list = NULL;
}

static void
sig_save_status(XMPP_SERVER_CONNECT_REC *conn,
    XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER_CONNECT(conn) || !IS_XMPP_SERVER(server)
	    || !server->connected)
		return;
	save_channels(server, conn);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server) || !server->connrec->reconnection)
		return;
	restore_channels(server);
}

void
muc_reconnect_init(void)
{
	signal_add_last("server connect copy", sig_conn_copy);
	signal_add("server reconnect remove", sig_conn_remove);
	signal_add("server reconnect save status", sig_save_status);
	signal_add_last("server connected", sig_connected);
}

void
muc_reconnect_deinit(void)
{
	signal_remove("server connect copy", sig_conn_copy);
	signal_remove("server reconnect remove", sig_conn_remove);
	signal_remove("server reconnect save status", sig_save_status);
	signal_remove("server connected", sig_connected);
}
