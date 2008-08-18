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

#include "loudmouth/loudmouth.h"

#include "module.h"
#include "settings.h"
#include "tools.h"

gboolean set_ssl(LmConnection *, GError **, gpointer);
gboolean set_proxy(LmConnection *, GError **);

void
xmpp_register(const char *host, int port, const gboolean use_ssl,
    const char *username, const char *password)
{
	LmConnection  *lmconn;
	LmMessage *lmsg, *reply;
	LmMessageNode *node;
	GError *error = NULL;
	char *recoded;

	g_debug("|%s|%d|%d|%s|%s|", host, port, use_ssl, username, password);

	g_debug("Registering account...");
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
	if (!lm_connection_open_and_block(lmconn, &error))
		goto err;
	lmsg = lm_message_new_with_sub_type(NULL,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", "jabber:iq:register");
	recoded = xmpp_recode_out(username);
	lm_message_node_add_child(node, "username", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(password);
	lm_message_node_add_child(node, "password", recoded);
	g_free(recoded);
	reply = lm_connection_send_with_reply_and_block(lmconn, lmsg, &error); 
	lm_message_unref(lmsg);
	if (reply == NULL)
		goto err;
	switch (lm_message_get_sub_type(reply)) {
	case LM_MESSAGE_SUB_TYPE_RESULT:
		g_debug("Account '%s' registered", username);
		/* TODO connect */
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
	default:
		node = lm_message_node_find_child(reply->node, "error");
		g_debug("Failed to register account '%s' (%s)", username,
		    node != NULL ? node->value : NULL);
	}
	lm_message_unref(reply);
	lm_connection_close(lmconn, NULL);
	lm_connection_unref(lmconn);
	return;

err:
	lm_connection_unref(lmconn);
	/* TODO display err */
	if (error != NULL) {
		g_debug("error: %s", error->message);
		g_error_free(error);
	}
}
