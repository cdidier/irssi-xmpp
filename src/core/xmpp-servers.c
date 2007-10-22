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

#include <sys/types.h>
#include <sys/socket.h>

#include "module.h"
#include "network.h"
#include "recode.h"
#include "settings.h"
#include "signals.h"
/*#include "servers-reconnect.h"*/

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"
#include "xmpp-tools.h"

gboolean
xmpp_server_is_alive(XMPP_SERVER_REC *server)
{
	return (server != NULL
	    && g_slist_find(servers, server) != NULL
	    && server->connected);
}

static int
isnickflag_func(char flag)
{
	return FALSE;
}

static int
ischannel_func(SERVER_REC *server, const char *data)
{
	return FALSE;
}

static const char
*get_nick_flags(void)
{
	return "";
}

static void
send_message(SERVER_REC *server, const char *target, const char *msg,
    int target_type)
{
	char *recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(target != NULL);
	g_return_if_fail(msg != NULL);

	if (!IS_XMPP_SERVER(server))
		return;
	
	/* ugly from irssi: recode the sent message back */
	recoded = recode_in(server, msg, target);

	if (target_type == SEND_TARGET_CHANNEL)
		xmpp_channel_send_message(XMPP_SERVER(server), target, recoded);
	else
		xmpp_send_message(XMPP_SERVER(server), target, recoded);

	g_free(recoded);
}

static void
xmpp_server_cleanup(XMPP_SERVER_REC *server)
{
	g_return_if_fail(IS_XMPP_SERVER(server));

	if (server->connected) {
		server_disconnect(SERVER(server));
		return;
	}

	if (lm_connection_is_open(server->lmconn))
		lm_connection_close(server->lmconn, NULL);

	lookup_servers = g_slist_remove(lookup_servers, server);
	servers = g_slist_remove(servers, server);

	lm_connection_unref(server->lmconn);

	g_free(server->nickname);
	g_free(server->jid);
	g_free(server->user);
	g_free(server->host);
	g_free(server->resource);
	g_free(server->ping_id);
}


SERVER_REC *
xmpp_server_init_connect(SERVER_CONNECT_REC *conn)
{
	XMPP_SERVER_REC *server;
	char *str;

	g_return_val_if_fail(IS_XMPP_SERVER_CONNECT(conn), NULL);
	if (conn->address == NULL || conn->address[0] == '\0')
		return NULL;
	if (conn->nick == NULL || conn->nick[0] == '\0') return NULL;

	server = g_new0(XMPP_SERVER_REC, 1);
	server->chat_type = XMPP_PROTOCOL;

	/* extract user informations */
	str = conn->nick;

	server->user = xmpp_extract_user(str);
	server->host = g_strdup(conn->address);
	server->jid = xmpp_jid_have_address(str) ? xmpp_strip_resource(str) :
	    g_strconcat(server->user, "@", server->host, NULL);
	server->resource = xmpp_extract_resource(str);
	if (server->resource == NULL)
		server->resource = g_strdup("irssi-xmpp");

	g_free(str);

	/* init xmpp's properties */
	server->priority = settings_get_int("xmpp_priority");
	if (xmpp_priority_out_of_bound(server->priority))
		server->priority = 0;
	server->default_priority = TRUE;

	server->ping_id = NULL;
	server->features = 0;
	server->roster = NULL;
	server->hmessage = NULL;
	server->hpresence = NULL;
	server->hiq = NULL;

	/* fill connrec record */
	server->connrec = (XMPP_SERVER_CONNECT_REC *)conn;
	server_connect_ref(conn);

	/* don't use irssi's sockets */
	server->connrec->no_connect = TRUE;

	if (server->connrec->port <= 0)
		server->connrec->port = (server->connrec->use_ssl) ?
		    LM_CONNECTION_DEFAULT_PORT_SSL : LM_CONNECTION_DEFAULT_PORT;

	server->connrec->nick =
	    g_strdup(settings_get_bool("xmpp_set_nick_as_username") ?
	    server->user : server->jid);
	server->nickname = g_strdup(server->connrec->nick);

	/* init loudmouth connection structure */
	server->lmconn = lm_connection_new(NULL);
	lm_connection_set_server(server->lmconn, server->connrec->address);
	lm_connection_set_port(server->lmconn, server->connrec->port);
	lm_connection_set_jid(server->lmconn, server->jid);
	lm_connection_set_keep_alive_rate(server->lmconn, 30);

	server_connect_init((SERVER_REC *)server);
	return (SERVER_REC *)server;
}

static LmSSLResponse
xmpp_server_ssl_cb(LmSSL *ssl, LmSSLStatus status, gpointer user_data)
{
	XMPP_SERVER_REC *server = XMPP_SERVER(user_data);

	switch (status) {
	case LM_SSL_STATUS_NO_CERT_FOUND:
		signal_emit("xmpp server status", 2, server,
			"SSL: No certificate found!");
		break;
	case LM_SSL_STATUS_UNTRUSTED_CERT:
		signal_emit("xmpp server status", 2, server,
			"SSL: Certificate is not trusted!");
		break;
	case LM_SSL_STATUS_CERT_EXPIRED:
		signal_emit("xmpp server status", 2, server,
			"SSL: Certificate has expired!");
		break;
	case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
		signal_emit("xmpp server status", 2, server,
			"SSL: Certificate has not been activated!");
		break;
	case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
		signal_emit("xmpp server status", 2, server,
			"SSL: Certificate hostname does not match expected hostname!");
		break;
	case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: 
		signal_emit("xmpp server status", 2, server,
			"SSL: Certificate fingerprint does not match expected fingerprint!");
		break;
	case LM_SSL_STATUS_GENERIC_ERROR:
		/*signal_emit("xmpp server status", 2, server,
			"SSL: Generic SSL error!");*/
		break;
	}

	return LM_SSL_RESPONSE_CONTINUE;
}

static void
xmpp_server_close_cb(LmConnection *connection, LmDisconnectReason reason,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;
	const char *msg;
		
	server = XMPP_SERVER(user_data);
	if (server != NULL)
		return;

	/* normal disconnection */
	if (reason == LM_DISCONNECT_REASON_OK)
		return;

	/* connection lost */
	server->connection_lost = TRUE;
	server_disconnect(SERVER(server));
}

static void
xmpp_server_auth_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;
	LmMessage *msg;
	LmMessageNode *query;
	char *priority;
	
	server = XMPP_SERVER(user_data);
	if (server == NULL)
		return;

	if (!success)
		goto err;

	signal_emit("xmpp server status", 2, server,
	    "Authenticated successfully.");

	/* fetch the roster */
	signal_emit("xmpp server status", 2, server, "Requesting the roster.");
	msg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	query = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query, "xmlns", XMLNS_ROSTER);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	/* discover server's features */
	msg = lm_message_new_with_sub_type(server->host, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	query = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query, "xmlns", XMLNS_DISCO_INFO);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	/* set presence available */
	signal_emit("xmpp server status", 2, server,
	    "Sending available presence message.");
	msg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE,
	    LM_MESSAGE_SUB_TYPE_AVAILABLE);

	priority = g_strdup_printf("%d", server->priority);
	lm_message_node_add_child(msg->node, "priority", priority);
	g_free(priority);

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
	
	server->show = XMPP_PRESENCE_AVAILABLE;

	lookup_servers = g_slist_remove(lookup_servers, server);
	server_connect_finished(SERVER(server));
	signal_emit("event connected", 1, server);
	return;

err:
	server_connect_failed(SERVER(server), "Authentication failed");
}

static void
xmpp_server_open_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;
	IPADDR ip;
	char *host;
	GError *error = NULL;

	server = XMPP_SERVER(user_data);
	if (server == NULL)
		return;

	if (!success)
		goto err;

	/* get the server address */
	host = lm_connection_get_local_host(server->lmconn);
	if (host != NULL) {
		net_host2ip(host, &ip);
		signal_emit("server connecting", 2, server, &ip);
		g_free(host);
	} else
		signal_emit("server connecting", 1, server);

	if (!lm_connection_authenticate(connection, server->user,
	    server->connrec->password, server->resource,
	    (LmResultFunction)xmpp_server_auth_cb, server, NULL, &error))
		goto err;

	return;

err:
	server->connection_lost = TRUE;
	server_connect_failed(SERVER(server),
	    (error != NULL) ? error->message : "Connection failed");
	g_free(error);
}

void
xmpp_server_connect(SERVER_REC *server)
{
	XMPP_SERVER_REC *xmppserver;
	LmSSL *ssl;
	GError *error = NULL;

	xmppserver = XMPP_SERVER(server);
	if (xmppserver == NULL)
		return;

	/* SSL */
	if (xmppserver->connrec->use_ssl) {
		if (!lm_ssl_is_supported()) {
			error = g_new(GError, 1);
			error->message = "SSL is not supported in this build.";
			goto err;
		}

		ssl = lm_ssl_new(NULL, (LmSSLFunction)xmpp_server_ssl_cb,
		    server, NULL);
		lm_connection_set_ssl(xmppserver->lmconn, ssl);
		lm_ssl_unref(ssl);
	} 

	/* Proxy: not supported yet */

	lm_connection_set_disconnect_function(xmppserver->lmconn,
	    (LmDisconnectFunction)xmpp_server_close_cb, (gpointer)server,
	    NULL);

	lookup_servers = g_slist_append(lookup_servers, server);
	signal_emit("server looking", 1, server);

	if (!lm_connection_open(xmppserver->lmconn, 
	    (LmResultFunction)xmpp_server_open_cb, (gpointer)server, NULL,
	    &error))
		goto err;

	return;

err:
	if (!IS_SERVER(server)) {
		g_free(error);
		return;
	}

	server->connection_lost = TRUE;
	server_connect_failed(SERVER(server),
	    (error != NULL) ? error->message : NULL);
	g_free(error);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	server->channels_join = (void *)xmpp_channels_join;
	server->isnickflag = (void *)isnickflag_func;
	server->ischannel = ischannel_func;
	server->get_nick_flags = (void *)get_nick_flags;
	server->send_message = send_message;
	
	server->connected = TRUE;
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
sig_server_disconnected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	xmpp_server_cleanup(server);
}

static void
sig_server_disconnected_first(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	/* failed server, if not connected */
	if (!server->connected) {
		server_connect_failed(SERVER(server), NULL);
		signal_stop();
	}
}

static void
sig_server_connect_failed(XMPP_SERVER_REC *server, char *msg)
{
	if (!IS_XMPP_SERVER(server))
		return;

	xmpp_server_cleanup(server);
}

static void
sig_server_quit(XMPP_SERVER_REC *server, char *reason)
{
	LmMessage *msg;
	char *status_recoded;

	if (!IS_XMPP_SERVER(server))
		return;
	
	msg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE,
	    LM_MESSAGE_SUB_TYPE_UNAVAILABLE);

	status_recoded = xmpp_recode_out((reason != NULL) ?
	    reason : settings_get_str("quit_message"));
	lm_message_node_add_child(msg->node, "status", status_recoded);
	g_free(status_recoded);

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

void
xmpp_servers_init(void)
{
	signal_add_first("server connected", (SIGNAL_FUNC)sig_connected);
	signal_add("server disconnected",
	    (SIGNAL_FUNC)sig_server_disconnected);
	signal_add_first("server disconnected",
	    (SIGNAL_FUNC)sig_server_disconnected_first);
	signal_add("server connect failed",
	    (SIGNAL_FUNC)sig_server_connect_failed);
	signal_add("server quit", (SIGNAL_FUNC)sig_server_quit);
	signal_add("server connect copy",
	    (SIGNAL_FUNC)sig_server_connect_copy);

	settings_add_bool("xmpp", "xmpp_set_nick_as_username", FALSE);
}

void
xmpp_servers_deinit(void)
{
	GSList *tmp, *next;

	/* disconnect all servers before unloading the module */
	for (tmp = lookup_servers; tmp != NULL; tmp = next) {
		next = tmp->next;
		if (IS_XMPP_SERVER(tmp->data))
			server_connect_failed(SERVER(tmp->data), NULL);
	}
	for (tmp = servers; tmp != NULL; tmp = next) {
		next = tmp->next;
		if (IS_XMPP_SERVER(tmp->data))
			server_disconnect(SERVER(tmp->data));
	}

	signal_remove("server connected", (SIGNAL_FUNC)sig_connected);
	signal_remove("server disconnected",
	    (SIGNAL_FUNC)sig_server_disconnected);
	signal_remove("server disconnected",
	    (SIGNAL_FUNC)sig_server_disconnected_first);
	signal_remove("server connect failed",
	    (SIGNAL_FUNC)sig_server_connect_failed);
	signal_remove("server quit", (SIGNAL_FUNC)sig_server_quit);
	signal_remove("server connect copy",
	    (SIGNAL_FUNC)sig_server_connect_copy);
}
