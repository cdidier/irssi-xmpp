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

#include <sys/utsname.h>

#include "module.h"
#include "signals.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"

char *
xmpp_recode(const char *str, const int mode)
{
	const char *charset = "UTF-8";
	G_CONST_RETURN char *local_charset;
	char *recoded;
	gboolean is_utf8 = FALSE;

	g_return_val_if_fail(str != NULL, NULL);

	local_charset = settings_get_str("term_charset");
	if (local_charset == NULL)
		is_utf8 = g_get_charset(&local_charset);

	if (is_utf8 || g_strcasecmp(local_charset, charset) == 0)
		goto avoid;

	if (mode == XMPP_RECODE_IN)
		recoded = g_convert(str, -1, local_charset, charset, NULL,
		    NULL, NULL);
	else if (mode == XMPP_RECODE_OUT)
		recoded = g_convert(str, -1, charset, local_charset, NULL,
		    NULL, NULL);
	else
		goto avoid;

	if (recoded == NULL)
		goto avoid;

	return recoded;

avoid:
	return g_strdup(str);
}

char *
xmpp_jid_get_ressource(const char *jid)
{
	char *pos;

	g_return_val_if_fail(jid != NULL, NULL);

	pos = g_utf8_strchr(jid, -1, '/');
	if (pos != NULL)
		return g_strdup(pos + 1);
	else
		return NULL;
}

char *
xmpp_jid_strip_ressource(const char *jid)
{
	char *pos;

	g_return_val_if_fail(jid != NULL, NULL);

	pos = g_utf8_strchr(jid, -1, '/');
	if (pos != NULL)
		return g_strndup(jid, pos - jid);
	else
		return g_strdup(jid);
}

char *
xmpp_jid_get_username(const char *jid)
{
	char *pos;

	g_return_val_if_fail(jid != NULL, NULL);

  	pos = g_utf8_strchr(jid, -1, '@');
	if (pos != NULL)
		return g_strndup(jid, pos - jid);
	else
		return xmpp_jid_strip_ressource(jid);
}

gboolean
xmpp_jid_have_ressource(const char *jid)
{
	g_return_val_if_fail(jid != NULL, NULL);

	return (g_utf8_strchr(jid, -1, '/') != NULL) ? TRUE : FALSE;
}

gboolean
xmpp_jid_have_address(const char *jid)
{
	g_return_val_if_fail(jid != NULL, NULL);

	return (g_utf8_strchr(jid, -1, '@') != NULL) ? TRUE : FALSE;
}

gboolean
xmpp_priority_out_of_bound(const int priority)
{
	return (-128 <= priority && priority <= 127) ? FALSE : TRUE;
}

void
xmpp_send_message_chat(XMPP_SERVER_REC *server, const char *to_jid,
    const char *message)
{
	LmMessage *msg;
	char *to_jid_recoded, *message_recoded;
	GError *error;

	g_return_if_fail(server != NULL);
	g_return_if_fail(to_jid != NULL);
	g_return_if_fail(message != NULL);

	to_jid_recoded = xmpp_recode(to_jid, XMPP_RECODE_OUT);
	message_recoded = xmpp_recode(message, XMPP_RECODE_OUT);
	error = NULL;

	msg = lm_message_new_with_sub_type(to_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	lm_message_node_add_child(msg->node, "body", message_recoded);

	lm_connection_send(server->lmconn, msg, &error);
	lm_message_unref(msg);

	if (error != NULL) {
		signal_emit("xmpp message error", 3, server, to_jid,
		    error->message);
		g_free(error);
	}

	g_free(to_jid_recoded);
	g_free(message_recoded);
}

void
xmpp_set_presence(XMPP_SERVER_REC *server, const int show, const char *status)
{
	LmMessage *msg;
	char *status_recoded, *priority;

	g_return_if_fail(server != NULL);

	msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);

	switch (show) {
	case XMPP_PRESENCE_AWAY:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_AWAY]);
		break;

	case XMPP_PRESENCE_CHAT:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_CHAT]); 
		break;

	case XMPP_PRESENCE_DND:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_DND]);
		break;

	case XMPP_PRESENCE_XA:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_XA]);
		break;

	default:
		/* unaway */
		if (server->usermode_away)
			signal_emit("event 305", 2, server, server->nick);

		server->show = XMPP_PRESENCE_AVAILABLE;
		g_free_and_null(server->away_reason);
	}


	/* away */
	if (lm_message_node_get_child(msg->node, "show") != NULL) {
		signal_emit("event 306", 2, server, server->nick);

		server->show = show;
		g_free(server->away_reason);
		server->away_reason = g_strdup(status);

		if (server->away_reason != NULL) {
			status_recoded = xmpp_recode(server->away_reason,
			    XMPP_RECODE_OUT);
			lm_message_node_add_child(msg->node, "status",
			    status_recoded);
			g_free(status_recoded);
		}
	}

	priority = g_strdup_printf("%d", server->priority);
	lm_message_node_add_child(msg->node, "priority", priority);
	g_free(priority);

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
}

/*
 * XEP-0092: Software Version
 * http://www.xmpp.org/extensions/xep-0092.html
 */
void
xmpp_version_send(XMPP_SERVER_REC *server, const char *to_jid,
    const char *id)
{
	LmMessage *msg;
	LmMessageNode *query_node;
	struct utsname u;

	g_return_if_fail(server != NULL);
	g_return_if_fail(to_jid != NULL);

	if (!settings_get_bool("xmpp_send_version"))
		return;

	msg = lm_message_new_with_sub_type(to_jid, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);

	if (id != NULL)
		lm_message_node_set_attribute(msg->node, "id", id);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:version");

	lm_message_node_add_child(query_node, "name", IRSSI_XMPP_PACKAGE);
	lm_message_node_add_child(query_node, "version", IRSSI_XMPP_VERSION);

	if (uname(&u) == 0)
		lm_message_node_add_child(query_node, "os", u.sysname);

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
}

/*
 * Incoming messages handlers
 */
static LmHandlerResult
xmpp_handle_message(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	LmMessageNode *body_node;
	const char *jid;
	char *message_recoded;

	server = XMPP_SERVER(user_data);

#ifdef DEBUG
	signal_emit("xmpp debug", 2, server,
	    lm_message_node_to_string(msg->node));
#endif

	if (lm_message_get_sub_type(msg) == LM_MESSAGE_SUB_TYPE_ERROR)
		goto err;

	body_node = lm_message_node_get_child(msg->node, "body");
	if (body_node == NULL)
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	message_recoded = xmpp_recode(body_node->value, XMPP_RECODE_IN);
	jid = lm_message_node_get_attribute(msg->node, "from");

	signal_emit("message private", 4, server, message_recoded, jid, jid);

	g_free(message_recoded);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;

err:
	signal_emit("xmpp message error", 3, server,
	    lm_message_node_get_attribute(msg->node, "from"),
	    lm_message_node_get_attribute(
	    lm_message_node_get_child(msg->node, "error"), "code"));

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
xmpp_handle_presence(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	LmMessageNode *show_node, *status_node, *priority_node;
	char *jid_recoded, *status_recoded;
	LmMessageSubType msg_type;

	server = XMPP_SERVER(user_data);
	jid_recoded = xmpp_recode(lm_message_node_get_attribute(msg->node,
	    "from"), XMPP_RECODE_IN);
	status_recoded = NULL;

#ifdef DEBUG
	signal_emit("xmpp debug", 2, server,
	    lm_message_node_to_string(msg->node));
#endif

	msg_type = lm_message_get_sub_type(msg);
	switch (msg_type) {

	/* the user is disconnecting */
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		status_node = lm_message_node_get_child(msg->node, "status");
		if (status_node != NULL)
			status_recoded = xmpp_recode(status_node->value,
			    XMPP_RECODE_IN);

		xmpp_roster_presence_unavailable(server, jid_recoded,
		    status_recoded);

		g_free(status_recoded);
		break;

	/* an error occured when the server try to get the pressence */
	case LM_MESSAGE_SUB_TYPE_ERROR:
		xmpp_roster_presence_error(server, jid_recoded);
		break;

	/* the user wants to add you in his/her roster */
	case LM_MESSAGE_SUB_TYPE_SUBSCRIBE:
		status_node = lm_message_node_get_child(msg->node, "status");
		if (status_node != NULL)
			status_recoded = xmpp_recode(status_node->value,
			    XMPP_RECODE_IN);

		signal_emit("xmpp jid subscribe", 3, server, jid_recoded,
		    status_recoded);

		g_free(status_recoded);
		break;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE:
		signal_emit("xmpp jid unsubscribe", 2, server, jid_recoded);
		break;

	case LM_MESSAGE_SUB_TYPE_SUBSCRIBED:
		signal_emit("xmpp jid subscribed", 2, server, jid_recoded);
		break;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED:
		signal_emit("xmpp jid unsubscribed", 2, server, jid_recoded);
		break;

	/* the user change his presence */
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:

		status_node = lm_message_node_get_child(msg->node, "status");
		if (status_node != NULL)
			status_recoded = xmpp_recode(status_node->value,
			    XMPP_RECODE_IN);

		show_node = lm_message_node_get_child(msg->node, "show");
		priority_node = lm_message_node_get_child(msg->node,
		    "priority");

		xmpp_roster_presence_update(server, jid_recoded,
		    (show_node != NULL) ? show_node->value : NULL,
		    status_recoded,
		    (priority_node != NULL) ? priority_node->value : NULL);

		g_free(status_recoded);
		break;

	default:
		g_free(jid_recoded);
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	}

	g_free(jid_recoded);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
xmpp_handle_iq(LmMessageHandler *handler, LmConnection *connection,
	LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	LmMessageNode *root, *query;
	const char *xmlns, *id, *jid;
	LmMessageSubType msg_type;

	server = XMPP_SERVER(user_data);
	root = lm_message_get_node(msg);
	jid = lm_message_node_get_attribute(msg->node, "from");
	id = lm_message_node_get_attribute(msg->node, "id");

#ifdef DEBUG
	signal_emit("xmpp debug", 2, server,
	    lm_message_node_to_string(msg->node));
#endif

	query = lm_message_node_get_child(msg->node, "query");
	if (query == NULL)
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	xmlns = lm_message_node_get_attribute(query, "xmlns");
	if (xmlns == NULL)
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

	msg_type = lm_message_get_sub_type(msg);
	switch (msg_type) {

	case LM_MESSAGE_SUB_TYPE_GET:
		if (g_ascii_strcasecmp(xmlns, "jabber:iq:version") == 0)
		    xmpp_version_send(server, jid, id);
		break;

	case LM_MESSAGE_SUB_TYPE_RESULT:
	case LM_MESSAGE_SUB_TYPE_SET :
		if (g_ascii_strcasecmp(xmlns, "jabber:iq:roster") == 0)
			xmpp_roster_update(server, query);
		break;
	
	default:
		return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;
	}

	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

void
xmpp_register_handlers(XMPP_SERVER_REC *server)
{
	LmMessageHandler *hmessage, *hpresence, *hiq;

	/* handle message */
	hmessage = lm_message_handler_new(
	    (LmHandleMessageFunction)xmpp_handle_message, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn, hmessage,
	    LM_MESSAGE_TYPE_MESSAGE, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hmessage);

	/* handle presence */
	hpresence = lm_message_handler_new(
	    (LmHandleMessageFunction)xmpp_handle_presence, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn, hpresence,
	    LM_MESSAGE_TYPE_PRESENCE, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hpresence);

	/* handle iq */
	hiq = lm_message_handler_new(
	    (LmHandleMessageFunction)xmpp_handle_iq, (gpointer)server, NULL);
	lm_connection_register_message_handler(server->lmconn, hiq,
	    LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hiq);
}
