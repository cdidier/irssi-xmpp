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

#include <stdlib.h>
#include <string.h>

#include "loudmouth/loudmouth.h"

#include "module.h"
#include "settings.h"
#include "signals.h"
#include "tools.h"

#include "loudmouth-tools.h"

#define XMLNS_REGISTER "jabber:iq:register"

#define REGISTER_FAIL_UNAUTHORIZED	"Registration unauthorized"
#define REGISTER_FAIL_UNAVAILABLE	"Service unavailable"
#define REGISTER_FAIL_CONFLICT		"Account already exists"
#define REGISTER_FAIL_TIMEOUT		"Connection times out"
#define REGISTER_FAIL_ERROR		"Cannot register account"
#define REGISTER_ERROR_INFOS		"Cannot send informations"
#define REGISTER_ERROR_CONNECTION	"Cannot open connection"

gboolean set_ssl(LmConnection *, GError **, gpointer);
gboolean set_proxy(LmConnection *, GError **);

struct register_data {
	char	*username;
	char	*host;
	char	*password;
	char 	*id;
	LmConnection		*lmconn;
	LmMessageHandler	*handler;
};

GSList *register_data;

static void
rd_cleanup(struct register_data *rd)
{
	register_data = g_slist_remove(register_data, rd);
	g_free(rd->username);
	g_free(rd->host);
	g_free(rd->password);
	g_free(rd->id);
	if (rd->handler != NULL) {
		if (lm_message_handler_is_valid(rd->handler))
			lm_message_handler_invalidate(rd->handler);
		lm_message_handler_unref(rd->handler);
	}
	if (lm_connection_get_state(rd->lmconn) != LM_CONNECTION_STATE_CLOSED)
		lm_connection_close(rd->lmconn, NULL);
	lm_connection_unref(rd->lmconn);
	g_free(rd);
}

static LmHandlerResult
handle_register(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *lmsg, gpointer user_data)
{
	LmMessageNode *node;
	struct register_data *rd;
	const char *id;
	char *reason;

	rd = user_data;
	id = lm_message_node_get_attribute(lmsg->node, "id");
	if (id == NULL || (id != NULL && strcmp(id, rd->id) != 0))
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	if ((node = lm_message_node_get_child (lmsg->node, "error")) != NULL) {
		switch (atoi(lm_message_node_get_attribute (node, "code"))) {
		case 401: /* Not Authorized */
		case 407: /* Registration Required */
			reason = REGISTER_FAIL_UNAUTHORIZED;
			break;
		case 501: /* Not Implemented */
		case 503: /* Service Unavailable */
			reason = REGISTER_FAIL_UNAVAILABLE;
			break;
		case 409: /* Conflict */
			reason = REGISTER_FAIL_CONFLICT;
			break;
		case 408: /* Request Timeout */
		case 504: /* Remote Server Timeout */
			reason = REGISTER_FAIL_TIMEOUT;
			break;
		default:
			reason = REGISTER_FAIL_ERROR;
		}
		signal_emit("xmpp register failed", 3, rd->username, rd->host,
		    reason);
	} else
		signal_emit("xmpp register succed", 2, rd->username, rd->host);
	rd_cleanup(rd);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
send_register(struct register_data *rd)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	GError *error = NULL;
	char *recoded;

	lmsg = lm_message_new_with_sub_type(NULL,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_REGISTER);
	recoded = xmpp_recode_out(rd->username);
	lm_message_node_add_child(node, "username", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(rd->password);
	lm_message_node_add_child(node, "password", recoded);
	g_free(recoded);
	rd->id = g_strdup(lm_message_node_get_attribute(lmsg->node, "id"));
	if (!lm_connection_send_with_reply(rd->lmconn, lmsg, rd->handler,
	    &error)) {
		signal_emit("xmpp register error", 3, rd->username, rd->host,
		    error != NULL ? error->message : REGISTER_ERROR_INFOS);
		if (error != NULL)
			g_error_free(error);
	}
	lm_message_unref(lmsg);
}

static void
lm_close_cb(LmConnection *connection, LmDisconnectReason reason,
    gpointer user_data)
{
	struct register_data *rd;

	if (reason == LM_DISCONNECT_REASON_OK)
		return;
	rd = user_data;
	signal_emit("xmpp register error", 3, rd->username, rd->host, NULL);
	rd_cleanup(rd);
}

static void
lm_open_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	struct register_data *rd;
	
	rd = user_data;
	if (!success)
		goto err;
	rd->handler = lm_message_handler_new(handle_register, rd, NULL);
	send_register(rd);
	return;

err:
	signal_emit("xmpp register error", 3, rd->username, rd->host, 
	    REGISTER_ERROR_CONNECTION);
	rd_cleanup(rd);
}

void
xmpp_register(const char *server, int port, const gboolean use_ssl,
    const char *username, const char *host, const char *password)
{
	LmConnection  *lmconn;
	GError *error = NULL;
	struct register_data *rd;

	lmconn = lm_connection_new(NULL);
	if (use_ssl && !set_ssl(lmconn, &error, NULL))
		goto err;
	if (settings_get_bool("xmpp_use_proxy") && !set_proxy(lmconn, &error))
		goto err;
	if (port <= 0)
		port = use_ssl ? LM_CONNECTION_DEFAULT_PORT_SSL
		    : LM_CONNECTION_DEFAULT_PORT;
	lm_connection_set_server(lmconn, host);
	lm_connection_set_port(lmconn, port);
	lm_connection_set_jid(lmconn, NULL);
	rd = g_new0(struct register_data, 1);
	rd->username = g_strdup(username);
	rd->host = g_strdup(host);
	rd->password = g_strdup(password);
	rd->id = NULL;
	rd->lmconn = lmconn;
	rd->handler = NULL;
	register_data = g_slist_prepend(register_data, rd);
	lm_connection_set_disconnect_function(lmconn, lm_close_cb, rd, NULL);
	if (!lm_connection_open(lmconn, lm_open_cb, rd, NULL, &error)) {
		rd_cleanup(rd);
		signal_emit("xmpp register error", 3, rd->username, rd->host,
		    error != NULL ? error->message : NULL);
		if (error != NULL)
			g_error_free(error);
	}
	return;

err:
	signal_emit("xmpp register error", 3, username, host,
	    error != NULL ? error->message : NULL);
	if (error != NULL)
		g_error_free(error);
	lm_connection_unref(lmconn);
}

void
register_init(void)
{
	register_data = NULL;
}

void
register_deinit(void)
{
	GSList *tmp, *next;

	for (tmp = register_data; tmp != NULL; tmp = next) {
		next = tmp->next;
		rd_cleanup((struct register_data *)tmp->data);
	}
}
