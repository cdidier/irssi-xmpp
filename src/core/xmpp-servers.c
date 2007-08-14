/*
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
#include "rawlog.h"
#include "channels-setup.h"
#include "net-sendbuffer.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"

static void
channels_join(SERVER_REC *server, const char *channel, int automatic)
{
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
    XMPP_SERVER_REC *xmppserver;

    if (!IS_XMPP_SERVER(server))
        return;

    g_return_if_fail(server != NULL);
    g_return_if_fail(target != NULL);
    g_return_if_fail(msg != NULL);
    xmppserver = XMPP_SERVER(server);
    
    if (target_type == SEND_TARGET_CHANNEL) {
        /* channel message */
    } else {
        /* private message */
        xmpp_send_message_chat(xmppserver, target, msg);
    }
}

static void
xmpp_server_cleanup(XMPP_SERVER_REC *server)
{
    if (lm_connection_is_open(server->lmconn))
        lm_connection_close(server->lmconn, NULL);

    lookup_servers = g_slist_remove(lookup_servers, server);
    servers = g_slist_remove(servers, server);

    lm_connection_unref(server->lmconn);
    g_free(server->ressource);

    xmpp_roster_cleanup(server);
}


SERVER_REC *
xmpp_server_init_connect(SERVER_CONNECT_REC *conn)
{
    XMPP_SERVER_REC *server;
    gchar *str;

    g_return_val_if_fail(IS_XMPP_SERVER_CONNECT(conn), NULL);
    if (conn->address == NULL || *conn->address == '\0') return NULL;
    if (conn->nick == NULL || *conn->nick == '\0') return NULL;

    server = g_new0(XMPP_SERVER_REC, 1);
    server->chat_type = XMPP_PROTOCOL;

    server->connrec = (XMPP_SERVER_CONNECT_REC *) conn;
    server_connect_ref(SERVER_CONNECT(conn));

    if (server->connrec->port <= 0) {
        if (server->connrec->use_ssl)
            server->connrec->port = LM_CONNECTION_DEFAULT_PORT_SSL;
        else
            server->connrec->port = LM_CONNECTION_DEFAULT_PORT;
    }
    
    /* don't use irssi's sockets */
    server->connrec->no_connect = TRUE;

    str = server->connrec->nick;

    server->ressource = xmpp_jid_get_ressource(server->connrec->nick);
    if (server->ressource == NULL)
        server->ressource = g_strdup("irssi-xmpp");

    server->connrec->nick = xmpp_jid_get_username(str);

    /* store the full jid */
    if (xmpp_jid_have_address(str))
        server->connrec->realname = xmpp_jid_strip_ressource(str);
    else
        server->connrec->realname = g_strdup_printf("%s@%s",
            server->connrec->nick, server->connrec->address);

    g_free(str);

    server->priority = settings_get_int("xmpp_priority");
    if (xmpp_priority_out_of_bound(server->priority))
        server->priority = 0;
    server->roster = NULL;

    server_connect_init((SERVER_REC *) server);

    /* init loudmouth connection structure */
    server->lmconn = lm_connection_new(NULL);
    lm_connection_set_server(server->lmconn, server->connrec->address);
    lm_connection_set_port(server->lmconn, server->connrec->port);
    lm_connection_set_jid(server->lmconn, server->connrec->realname);

    return (SERVER_REC *) server;
}

static LmSSLResponse
xmpp_server_ssl_cb(LmSSL *ssl, LmSSLStatus status, gpointer user_data)
{
    XMPP_SERVER_REC *server = XMPP_SERVER(user_data);

    switch (status) {
    case LM_SSL_STATUS_NO_CERT_FOUND:
        signal_emit("server connect status", 2, server,
            "SSL: No certificate found!");
        break;
    case LM_SSL_STATUS_UNTRUSTED_CERT:
        signal_emit("server connect status", 2, server,
            "SSL: Certificate is not trusted!");
        break;
    case LM_SSL_STATUS_CERT_EXPIRED:
        signal_emit("server connect status", 2, server,
            "SSL: Certificate has expired!");
        break;
    case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
        signal_emit("server connect status", 2, server,
            "SSL: Certificate has not been activated!");
        break;
    case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
        signal_emit("server connect status", 2, server,
            "SSL: Certificate hostname does not match expected hostname!");
        break;
    case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: 
        signal_emit("server connect status", 2, server,
            "SSL: Certificate fingerprint does not match expected fingerprint!");
        break;
    case LM_SSL_STATUS_GENERIC_ERROR:
        /*signal_emit("server connect status", 2, server,
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

    switch (reason) {
    case LM_DISCONNECT_REASON_OK:
        g_debug("ok");
        return;
    case LM_DISCONNECT_REASON_PING_TIME_OUT:
        msg = "Connection to the server timed out.";
        break;
    case LM_DISCONNECT_REASON_HUP:
        msg = "Connection was hung up.";
        break;
    case LM_DISCONNECT_REASON_ERROR:
        msg = "Error";
        break;
    case LM_DISCONNECT_REASON_UNKNOWN:
    default:
        msg = "Unknown error";
    }

    signal_emit("server disconnected", 2, server, msg);
}

static void
xmpp_server_auth_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
    XMPP_SERVER_REC *server;
    LmMessage *msg;
    LmMessageNode *query;
    gchar *priority;
    
    server = XMPP_SERVER(user_data);

    if (!success)
        goto err_auth;

    signal_emit("server connect status", 2, server,
        "Authenticated successfully.");

    /* fetch the roster */
    signal_emit("server connect status", 2, server, "Requesting the roster.");
    msg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_IQ,
        LM_MESSAGE_SUB_TYPE_GET);
    query = lm_message_node_add_child(lm_message_get_node(msg), "query", NULL);
    lm_message_node_set_attribute(query, "xmlns", "jabber:iq:roster");
    lm_connection_send(server->lmconn, msg, NULL);
    lm_message_unref(msg);

    /* set presence available */
    signal_emit("server connect status", 2, server,
        "Sending available presence message.");
    msg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE,
        LM_MESSAGE_SUB_TYPE_AVAILABLE);

    priority = g_strdup_printf("%d", server->priority);
    lm_message_node_add_child(lm_message_get_node(msg), "priority", priority);
    g_free(priority);

    lm_connection_send(server->lmconn, msg, NULL);
    lm_message_unref(msg);

	server->show = XMPP_PRESENCE_AVAILABLE;
    return;

err_auth:
    signal_emit("server connect failed", 2, server, "Authentication failed");
}

static void
xmpp_server_open_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
    XMPP_SERVER_REC *server;
    GError *error = NULL;

    server = XMPP_SERVER(user_data);

    if (!success)
        goto err_open;

    if (!lm_connection_authenticate(connection, server->connrec->nick,
                server->connrec->password, server->ressource,
                (LmResultFunction) xmpp_server_auth_cb, server, NULL, &error))
        goto err_open;

    lookup_servers = g_slist_remove(lookup_servers, server);
    servers = g_slist_append(servers, server);

    signal_emit("server connected", 1, server);
    return;

err_open:
    signal_emit("server connect failed", 2, server,
        error ? error->message : "Connection failed");
    g_free(error);
}

void
xmpp_server_connect(SERVER_REC *server)
{
    XMPP_SERVER_REC *xmppserver;
    LmSSL *ssl;
    GError *error = NULL;

    if (!IS_XMPP_SERVER(server))
        return;
    xmppserver = XMPP_SERVER(server);

    /* SSL */
    if (xmppserver->connrec->use_ssl) {
        if (!lm_ssl_is_supported ()) {
            signal_emit("server connect status", 2, server,
                "SSL is not supported in this build.");
            goto err_connect;
        }

        ssl = lm_ssl_new(NULL, (LmSSLFunction) xmpp_server_ssl_cb, server,
            NULL);
        lm_connection_set_ssl(xmppserver->lmconn, ssl);
        lm_ssl_unref(ssl);
    } 

    /* Proxy */

    xmpp_register_handlers(xmppserver);

    lm_connection_set_disconnect_function(xmppserver->lmconn,
        (LmDisconnectFunction) xmpp_server_close_cb, (gpointer) server, NULL);

    signal_emit("server looking", 1, server);

    if (!lm_connection_open(xmppserver->lmconn, 
            (LmResultFunction) xmpp_server_open_cb, (gpointer) server, NULL,
            &error))
        goto err_connect;

    lookup_servers = g_slist_append(lookup_servers, server);

    signal_emit("server connecting", 1, server);
    return;

err_connect:
    signal_emit("server connect failed", 2, server,
        error ? error->message : NULL);
    g_free(error);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
    if (!IS_XMPP_SERVER(server))
        return;

    server->channels_join = channels_join;
    server->isnickflag = (void *) isnickflag_func;
    server->ischannel = ischannel_func;
    server->get_nick_flags = (void *) get_nick_flags;
    server->send_message = send_message;
    
    /* connection to server finished, fill the rest of the fields */
    server->connected = TRUE;
    server->connect_time = time(NULL);

    signal_emit("event connected", 1, server);
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
    *dest = (SERVER_CONNECT_REC *) rec;
}

static void
sig_server_disconnected(XMPP_SERVER_REC *server)
{
    if (!IS_XMPP_SERVER(server))
        return;

    xmpp_server_cleanup(server);
}

static void
sig_connect_failed(XMPP_SERVER_REC *server, gchar *msg)
{
    if (!IS_XMPP_SERVER(server))
        return;

    xmpp_server_cleanup(server);
}

void
xmpp_servers_init(void)
{
    signal_add_first("server connected", (SIGNAL_FUNC) sig_connected);
    signal_add("server disconnected", (SIGNAL_FUNC) sig_server_disconnected);
    signal_add("server connect failed", (SIGNAL_FUNC) sig_connect_failed);
    signal_add("server connect copy", (SIGNAL_FUNC) sig_server_connect_copy);
}

void
xmpp_servers_deinit(void)
{
    signal_remove("server connected", (SIGNAL_FUNC) sig_connected);
    signal_remove("server disconnected",
        (SIGNAL_FUNC) sig_server_disconnected);
    signal_remove("server connect failed", (SIGNAL_FUNC) sig_connect_failed);
    signal_remove("server connect copy",
        (SIGNAL_FUNC) sig_server_connect_copy);
}
