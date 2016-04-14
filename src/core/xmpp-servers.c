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

#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <string.h>
#include <termios.h>

#include "module.h"
#include "network.h"
#include "recode.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "protocol.h"
#include "rosters-tools.h"
#include "tools.h"

static void
channels_join(SERVER_REC *server, const char *data, int automatic)
{
}

static int
isnickflag_func(SERVER_REC *server, char flag)
{
	return FALSE;
}

static int
ischannel_func(SERVER_REC *server, const char *data)
{
	return FALSE;
}

static const char *
get_nick_flags(SERVER_REC *server)
{
	return "";
}

static void
send_message(SERVER_REC *server, const char *target, const char *msg,
    int target_type)
{
	LmMessage *lmsg;
	char *str, *recoded;

	if (!IS_XMPP_SERVER(server))
		return;
	g_return_if_fail(target != NULL);
	g_return_if_fail(msg != NULL);
	if (target_type == SEND_TARGET_CHANNEL) {
		recoded = xmpp_recode_out(target);
		lmsg = lm_message_new_with_sub_type(recoded,
		    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	} else {
		str = rosters_resolve_name(XMPP_SERVER(server), target);
		recoded = xmpp_recode_out(str != NULL ? str : target);
		g_free(str);
		lmsg = lm_message_new_with_sub_type(recoded,
		    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	}
	g_free(recoded);
	/* ugly from irssi: recode the sent message back */
	str = recode_in(server, msg, target);
	recoded = xmpp_recode_out(str);
	g_free(str);
	lm_message_node_add_child(lmsg->node, "body", recoded);
	g_free(recoded);
	signal_emit("xmpp send message", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
server_cleanup(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;
	if (server->timeout_tag)
		g_source_remove(server->timeout_tag);
	if (lm_connection_get_state(server->lmconn) !=
	    LM_CONNECTION_STATE_CLOSED)
		lm_connection_close(server->lmconn, NULL);
	lm_connection_unref(server->lmconn);
	g_free(server->jid);
	g_free(server->user);
	g_free(server->domain);
	g_free(server->resource);
	g_free(server->ping_id);
}

SERVER_REC *
xmpp_server_init_connect(SERVER_CONNECT_REC *connrec)
{
	XMPP_SERVER_REC *server;
	XMPP_SERVER_CONNECT_REC *conn = (XMPP_SERVER_CONNECT_REC *)connrec;
	char *recoded;

	if (conn->address == NULL || conn->address[0] == '\0')
		return NULL;
	if (conn->nick == NULL || conn->nick[0] == '\0')
		return NULL;
	g_return_val_if_fail(IS_XMPP_SERVER_CONNECT(conn), NULL);

	server = g_new0(XMPP_SERVER_REC, 1);
	server->chat_type = XMPP_PROTOCOL;
	server->user = xmpp_extract_user(conn->nick);
	server->domain = xmpp_have_domain(conn->nick) ?
	    xmpp_extract_domain(conn->nick) : g_strdup(conn->address);
	server->jid = xmpp_have_domain(conn->nick) ?
	    xmpp_strip_resource(conn->nick)
	    : g_strconcat(server->user, "@", server->domain, (void *)NULL);
	server->resource = xmpp_extract_resource(conn->nick);
	if (server->resource == NULL)
		server->resource = g_strdup("irssi-xmpp");
	server->priority = settings_get_int("xmpp_priority");
	if (xmpp_priority_out_of_bound(server->priority))
		server->priority = 0;
	server->ping_id = NULL;
	server->server_features = NULL;
	server->my_resources = NULL;
	server->roster = NULL;
	server->msg_handlers = NULL;
	server->channels_join = channels_join;
	server->isnickflag = isnickflag_func;
	server->ischannel = ischannel_func;
	server->get_nick_flags = get_nick_flags;
	server->send_message = send_message;
	server->connrec = (XMPP_SERVER_CONNECT_REC *)conn;
	server_connect_ref(connrec);

	/* don't use irssi's sockets */
	server->connrec->no_connect = TRUE;
	server->connect_pid = -1;

	if (server->connrec->port <= 0)
		server->connrec->port = (server->connrec->use_ssl) ?
		    LM_CONNECTION_DEFAULT_PORT_SSL : LM_CONNECTION_DEFAULT_PORT;

	if (conn->real_jid == NULL)
		conn->real_jid = conn->nick;
	else
		g_free(conn->nick);
	conn->nick = g_strdup(settings_get_bool("xmpp_set_nick_as_username") ?
	    server->user : server->jid);

	/* init loudmouth connection structure */
	server->lmconn = lm_connection_new(NULL);
	lm_connection_set_server(server->lmconn, server->connrec->address);
	lm_connection_set_port(server->lmconn, server->connrec->port);
	recoded = xmpp_recode_out(server->jid);
	lm_connection_set_jid(server->lmconn, recoded);
	g_free(recoded);
	lm_connection_set_keep_alive_rate(server->lmconn,
	    settings_get_time("server_connect_timeout")/1000);

	server->timeout_tag = 0;
	server_connect_init((SERVER_REC *)server);
	server->connect_tag = 1;
	return (SERVER_REC *)server;
}

static LmSSLResponse
lm_ssl_cb(LmSSL *ssl, LmSSLStatus status, gpointer user_data)
{
	XMPP_SERVER_REC *server;

	if ((server = XMPP_SERVER(user_data)) == NULL)
		return LM_SSL_RESPONSE_CONTINUE;
	switch (status) {
	case LM_SSL_STATUS_NO_CERT_FOUND:
		g_warning("SSL (%s): no certificate found",
		    server->connrec->address);
		break;
	case LM_SSL_STATUS_UNTRUSTED_CERT:
		g_warning("SSL (%s): certificate is not trusted",
		    server->connrec->address);
		break;
	case LM_SSL_STATUS_CERT_EXPIRED:
		g_warning("SSL (%s): certificate has expired",
		    server->connrec->address);
		break;
	case LM_SSL_STATUS_CERT_NOT_ACTIVATED:
		g_warning("SSL (%s): certificate has not been activated",
		    server->connrec->address);
		break;
	case LM_SSL_STATUS_CERT_HOSTNAME_MISMATCH:
		g_warning("SSL (%s): certificate hostname does not match "
		    "expected hostname", server->connrec->address);
		break;
	case LM_SSL_STATUS_CERT_FINGERPRINT_MISMATCH: 
		g_warning("SSL (%s): certificate fingerprint does not match "
		    "expected fingerprint", server->connrec->address);
		break;
	case LM_SSL_STATUS_GENERIC_ERROR:
		g_warning("SSL (%s): generic error", server->connrec->address);
		break;
	}
	return LM_SSL_RESPONSE_CONTINUE;
}

static void
lm_close_cb(LmConnection *connection, LmDisconnectReason reason,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;

	if ((server = XMPP_SERVER(user_data)) == NULL || !server->connected
	    || reason == LM_DISCONNECT_REASON_OK)
		return;
	server->connection_lost = TRUE;
	server_disconnect(SERVER(server));
}

static void
lm_auth_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;
	
	if ((server = XMPP_SERVER(user_data)) == NULL)
		return;
	if (!success) {
		server_connect_failed(SERVER(server), "Authentication failed");
		return;
	}
	signal_emit("xmpp server status", 2, server,
	    "Authenticated successfully.");

	/* finnish connection process */
	lookup_servers = g_slist_remove(lookup_servers, server);
	g_source_remove(server->connect_tag);
	server->connect_tag = -1;
	server->show = XMPP_PRESENCE_AVAILABLE;
	server->connected = TRUE;
	if (server->timeout_tag) {
		g_source_remove(server->timeout_tag);
		server->timeout_tag = 0;
	}
	server_connect_finished(SERVER(server));
	server->real_connect_time = server->connect_time;
}

/*
 * Displays input prompt on command line and takes input data from user
 * From irssi-silc (silc-client/lib/silcutil/silcutil.c)
 */
static char *
get_password()
{
	char input[2048], *ret = NULL;
	int fd;

#ifndef DISABLE_TERMIOS
	struct termios to;
	struct termios to_old;

	if ((fd = open("/dev/tty", O_RDONLY)) < 0) {
		g_warning("Cannot open /dev/tty: %s\n",
		    strerror(errno));
		return NULL;
	}
    	signal(SIGINT, SIG_IGN);

	/* Get terminal info */
	tcgetattr(fd, &to);
	to_old = to;
	/* Echo OFF, and assure we can prompt and get input */
	to.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	to.c_lflag |= ICANON;
	to.c_iflag &= ~IGNCR;
	to.c_iflag |= ICRNL;
	to.c_cc[VMIN] = 255;
	tcsetattr(fd, TCSANOW, &to);

	printf("\tXMPP Password: ");
	fflush(stdout);

	memset(input, 0, sizeof(input));
	if ((read(fd, input, sizeof(input))) < 0) {
		g_warning("Cannot read from /dev/tty: %s\n",
		    strerror(errno));
		tcsetattr(fd, TCSANOW, &to_old);
		return NULL;
	}
	if (strlen(input) <= 1) {
		tcsetattr(fd, TCSANOW, &to_old);
		return NULL;
	}
	if ((ret = strchr(input, '\n')) != NULL)
		*ret = '\0';

	/* Restore old terminfo */
	tcsetattr(fd, TCSANOW, &to_old);
	signal(SIGINT, SIG_DFL);

	ret = g_strdup(input);
	memset(input, 0, sizeof(input));
#endif /* DISABLE_TERMIOS */
	return ret;
}

static void
lm_open_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	XMPP_SERVER_REC *server;
	IPADDR ip;
	char *host;
	char *recoded_user, *recoded_password, *recoded_resource;

	if ((server = XMPP_SERVER(user_data)) == NULL || !success)
		return;
	/* get the server address */
	host = lm_connection_get_local_host(server->lmconn);
	if (host != NULL) {
		net_host2ip(host, &ip);
		signal_emit("server connecting", 2, server, &ip);
		g_free(host);
	} else
		signal_emit("server connecting", 1, server);
	if (server->connrec->use_ssl)
		signal_emit("xmpp server status", 2, server, 
		    "Using SSL encryption.");
	else if (lm_ssl_get_use_starttls(lm_connection_get_ssl(server->lmconn)))
		signal_emit("xmpp server status", 2, server,
		    "Using STARTTLS encryption.");
	recoded_user = xmpp_recode_out(server->user);

	/* prompt for password or re-use typed password */
	if (server->connrec->prompted_password != NULL) {
		g_free_not_null(server->connrec->password);
		server->connrec->password =
		    g_strdup(server->connrec->prompted_password);
	} else if (server->connrec->password == NULL
	    || *(server->connrec->password) == '\0'
	    || *(server->connrec->password) == '\r') {
		g_free_not_null(server->connrec->password);
		server->connrec->prompted_password = get_password();
		signal_emit("send command", 1, "redraw");
		if (server->connrec->prompted_password != NULL)
			server->connrec->password =
			    g_strdup(server->connrec->prompted_password);
		else
			server->connrec->password = g_strdup("");
	}

	recoded_password = xmpp_recode_out(server->connrec->password);
	recoded_resource = xmpp_recode_out(server->resource);
	lm_connection_authenticate(connection, recoded_user,
	    recoded_password, recoded_resource, lm_auth_cb, server,
	    NULL, NULL);
	g_free(recoded_user);
	g_free(recoded_password);
	g_free(recoded_resource);
}

gboolean
set_ssl(LmConnection *lmconn, GError **error, gpointer user_data,
    gboolean use_starttls)
{
	LmSSL *ssl;

	if (!lm_ssl_is_supported() && error != NULL) {
		*error = g_new(GError, 1);
		(*error)->message =
		    g_strdup("SSL is not supported in this build");
		return FALSE;
	}
	ssl = lm_ssl_new(NULL, lm_ssl_cb, user_data, NULL);
	lm_connection_set_ssl(lmconn, ssl);
	if (use_starttls)
		lm_ssl_use_starttls(ssl, TRUE, TRUE);
	lm_ssl_unref(ssl);
	return TRUE;
}

gboolean
set_proxy(LmConnection *lmconn, GError **error)
{
	LmProxy *proxy;
	LmProxyType type;
	const char *str;
	char *recoded;

	str = settings_get_str("xmpp_proxy_type");
	if (str != NULL && g_ascii_strcasecmp(str, XMPP_PROXY_HTTP) == 0)
		type = LM_PROXY_TYPE_HTTP;
	else {
		if (error != NULL) {
			*error = g_new(GError, 1);
			(*error)->message = g_strdup("Invalid proxy type");
		}
		return FALSE;
	}
	str = settings_get_str("xmpp_proxy_address");
	if (str == NULL || *str == '\0') {
		if (error != NULL) {
			*error = g_new(GError, 1);
			(*error)->message =
			    g_strdup("Proxy address not specified");
		}
		return FALSE;
	}
	int port = settings_get_int("xmpp_proxy_port");
	if (port <= 0) {
		if (error != NULL) {
			*error = g_new(GError, 1);
			(*error)->message =
			    g_strdup("Invalid proxy port range");
		}
		return FALSE;
	}
	proxy = lm_proxy_new_with_server(type, str, port);
	str = settings_get_str("xmpp_proxy_user");
	if (str != NULL && *str != '\0') {
		recoded = xmpp_recode_out(str);
		lm_proxy_set_username(proxy, recoded);
		g_free(recoded);
	}
	str = settings_get_str("xmpp_proxy_password");
	if (str != NULL && *str != '\0') {
		recoded = xmpp_recode_out(str);
		lm_proxy_set_password(proxy, recoded);
		g_free(recoded);
	}
	lm_connection_set_proxy(lmconn, proxy);
	lm_proxy_unref(proxy);
	return TRUE;
}

static int
check_connection_timeout(XMPP_SERVER_REC *server)
{
	if (g_slist_find(lookup_servers, server) == NULL)
		return FALSE;
	if (!server->connected) {
		g_warning("%s: no response from server",
		    server->connrec->address);
		server->connection_lost = TRUE;
		server_disconnect(SERVER(server));
	} else
		server->timeout_tag = 0;
	return FALSE;
}

void
xmpp_server_connect(XMPP_SERVER_REC *server)
{
	GError *error;
	const char *err_msg;

	if (!IS_XMPP_SERVER(server))
		return;
	error = NULL;
	err_msg = NULL;
	if (server->connrec->use_ssl) {
		if (!set_ssl(server->lmconn, &error, server, FALSE)) {
			err_msg = "Cannot init ssl";
			goto err;
		}
	} else
		set_ssl(server->lmconn, &error, server, TRUE);
	if (settings_get_bool("xmpp_use_proxy")
	    && !set_proxy(server->lmconn, &error)) {
		err_msg = "Cannot set proxy";
		goto err;
	}
	lm_connection_set_disconnect_function(server->lmconn,
	    lm_close_cb, server, NULL);
	lookup_servers = g_slist_append(lookup_servers, server);
	signal_emit("server looking", 1, server);
	server->timeout_tag = g_timeout_add(
	    settings_get_time("server_connect_timeout"),
	    (GSourceFunc)check_connection_timeout, server);
	if (!lm_connection_open(server->lmconn,  lm_open_cb, server,
	    NULL, &error)) {
		err_msg = "Connection failed";
		goto err;
	}
	return;

err:
	server->connection_lost = TRUE;
	if (error != NULL) {
		server_connect_failed(SERVER(server), error->message);
		g_error_free(error);
	} else
		server_connect_failed(SERVER(server), err_msg);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	LmMessage *lmsg;
	char *str;

	if (!IS_XMPP_SERVER(server) || (server->connrec->reconnection
	    && xmpp_presence_changed(server->connrec->show, server->show,
	    server->connrec->away_reason, server->away_reason,
	    server->connrec->priority, server->priority)))
		return;
	/* set presence available */
	lmsg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE,
	    LM_MESSAGE_SUB_TYPE_AVAILABLE);
	str = g_strdup_printf("%d", server->priority);
	lm_message_node_add_child(lmsg->node, "priority", str);
	g_free(str);
	signal_emit("xmpp send presence", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_server_quit(XMPP_SERVER_REC *server, char *reason)
{
	LmMessage *lmsg;
	char *str;

	if (!IS_XMPP_SERVER(server))
		return;
	lmsg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_PRESENCE,
	    LM_MESSAGE_SUB_TYPE_UNAVAILABLE);
	str = xmpp_recode_out((reason != NULL) ?
	    reason : settings_get_str("quit_message"));
	lm_message_node_add_child(lmsg->node, "status", str);
	g_free(str);
	signal_emit("xmpp send presence", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
disconnect_all(void)
{
	GSList *tmp, *next;

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
}

static void
sig_session_save(void)
{
	/* We don't support /UPGRADE, so disconnect all servers
	 * before performing it. */
	disconnect_all();
}

void
xmpp_servers_init(void)
{
	signal_add_last("server connected", sig_connected);
	signal_add_last("server disconnected", server_cleanup);
	signal_add_last("server connect failed", server_cleanup);
	signal_add("server quit", sig_server_quit);
	signal_add_first("session save", sig_session_save);

	settings_add_int("xmpp", "xmpp_priority", 0);
	settings_add_int("xmpp", "xmpp_priority_away", -1);
	settings_add_bool("xmpp_lookandfeel", "xmpp_set_nick_as_username",
	    FALSE);
	settings_add_bool("xmpp_proxy", "xmpp_use_proxy", FALSE);
	settings_add_str("xmpp_proxy", "xmpp_proxy_type", "http");
	settings_add_str("xmpp_proxy", "xmpp_proxy_address", NULL);
	settings_add_int("xmpp_proxy", "xmpp_proxy_port", 8080);
	settings_add_str("xmpp_proxy", "xmpp_proxy_user", NULL);
	settings_add_str("xmpp_proxy", "xmpp_proxy_password", NULL);
}

void
xmpp_servers_deinit(void)
{
	/* disconnect all servers before unloading the module */
	disconnect_all();

	signal_remove("server connected", sig_connected);
	signal_remove("server disconnected", server_cleanup);
	signal_remove("server connect failed", server_cleanup);
	signal_remove("server quit", sig_server_quit);
	signal_remove("session save", sig_session_save);
}
