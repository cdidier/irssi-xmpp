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
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "tools.h"

static int message_types[] = {
	LM_MESSAGE_TYPE_MESSAGE,
	LM_MESSAGE_TYPE_PRESENCE,
	LM_MESSAGE_TYPE_IQ,
	-1
};

static void
send_stanza(XMPP_SERVER_REC *server, LmMessage *lmsg)
{
	char *xml, *recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(lmsg != NULL);
	xml = lm_message_node_to_string(lmsg->node);
	recoded = xmpp_recode_in(xml);
	g_free(xml);
	signal_emit("xmpp xml out", 2, server, recoded);
	g_free(recoded);
	lm_connection_send(server->lmconn, lmsg, NULL);
}

static LmHandlerResult
handle_stanza(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *lmsg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	int type;
	const char *id;
	char *from, *to, *raw, *xml;

	if ((server = XMPP_SERVER(user_data)) == NULL)
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	xml = lm_message_node_to_string(lmsg->node);
	raw = xmpp_recode_in(xml);
	signal_emit("xmpp xml in", 2, server, raw);
	g_free(xml);
	g_free(raw);
	type = lm_message_get_sub_type(lmsg);
	id = lm_message_node_get_attribute(lmsg->node, "id");
	if (id == NULL)
		id = "";
	from = xmpp_recode_in(lm_message_node_get_attribute(lmsg->node, "from"));
	if (from == NULL)
		from = g_strdup("");
	to = xmpp_recode_in(lm_message_node_get_attribute(lmsg->node, "to"));
	if (to == NULL)
		to = g_strdup("");
	switch(lm_message_get_type(lmsg)) {
	case LM_MESSAGE_TYPE_MESSAGE:
		signal_emit("xmpp recv message", 6,
		    server, lmsg, type, id, from, to);
		break;
	case LM_MESSAGE_TYPE_PRESENCE:
		signal_emit("xmpp recv presence", 6,
		    server, lmsg, type, id, from, to);
		break;
	case LM_MESSAGE_TYPE_IQ:
		signal_emit("xmpp recv iq", 6,
		    server, lmsg, type, id, from, to);
		break;
	default:
		signal_emit("xmpp recv others", 6,
		    server, lmsg, type, id, from, to);
		break;
	}
	g_free(from);
	g_free(to);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
free_message_handler(LmMessageHandler *h)
{
	if (lm_message_handler_is_valid(h))
		lm_message_handler_invalidate(h);
	lm_message_handler_unref(h);
}

static void
unregister_stanzas(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;
	g_slist_free_full(server->msg_handlers,
	    (GDestroyNotify)free_message_handler);
	server->msg_handlers = NULL;
}

static void
register_stanzas(XMPP_SERVER_REC *server)
{
	LmMessageHandler *h;
	int i;

	if (!IS_XMPP_SERVER(server))
		return;
	if (server->msg_handlers != NULL &&
	    g_slist_length(server->msg_handlers) != 0)
		unregister_stanzas(server);
	for(i = 0; message_types[i] != -1; ++i) {
		h = lm_message_handler_new(handle_stanza, server, NULL);
		lm_connection_register_message_handler(server->lmconn, h,
		    message_types[i], LM_HANDLER_PRIORITY_NORMAL);
		server->msg_handlers = g_slist_prepend(server->msg_handlers, h);
	}
}

void
stanzas_init(void)
{
	signal_add("server connecting", register_stanzas);
	signal_add_first("server disconnected", unregister_stanzas);
	signal_add_last("xmpp send message", send_stanza); 
	signal_add_last("xmpp send presence", send_stanza); 
	signal_add_last("xmpp send iq", send_stanza); 
	signal_add_last("xmpp send others", send_stanza); 
}

void
stanzas_deinit(void)
{
	signal_remove("server connecting", register_stanzas);
	signal_remove("server disconnected", unregister_stanzas);
	signal_remove("xmpp send message", send_stanza); 
	signal_remove("xmpp send presence", send_stanza); 
	signal_remove("xmpp send iq", send_stanza); 
	signal_remove("xmpp send others", send_stanza); 
}
