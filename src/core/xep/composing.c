/*
 * Copyright (C) 2007,2008,2009 Colin DIDIER
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

/*
 * XEP-0022: Message Events
 */

#include <string.h>

#include "module.h"
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "xmpp-queries.h"
#include "tools.h"
#include "tool_datalist.h"
#include "disco.h"

#define XMLNS_EVENT "jabber:x:event"

static DATALIST *composings;

#define send_start(server, dest, id) \
	send_composing_event(server, dest, id, TRUE)
#define send_stop(server, dest, id) \
	send_composing_event(server, dest, id, FALSE)

static void
send_composing_event(XMPP_SERVER_REC *server, const char *dest, const char *id,
    gboolean composing)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_EVENT);
	if (composing)
		lm_message_node_add_child(node, "composing", NULL);
	if (id != NULL)
		lm_message_node_add_child(node, "id", id);
	signal_emit("xmpp send message", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_composing_start(XMPP_SERVER_REC *server, const char *dest)
{
	DATALIST_REC *rec;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);
	if ((rec = datalist_find(composings, server, dest)) != NULL)
		send_start(server, dest, rec->data);
}

static void
sig_composing_stop(XMPP_SERVER_REC *server, const char *dest)
{
	DATALIST_REC *rec;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);
	if ((rec = datalist_find(composings, server, dest)) != NULL)
		send_stop(server, dest, rec->data);
}

static void
sig_composing_show(XMPP_SERVER_REC *server, const char *dest)
{
	XMPP_QUERY_REC *query;

	if ((query = xmpp_query_find(server, dest)) != NULL)
		query->composing_visible = TRUE;
}

static void
sig_composing_hide(XMPP_SERVER_REC *server, const char *dest)
{
	XMPP_QUERY_REC *query;

	if ((query = xmpp_query_find(server, dest)) != NULL)
		query->composing_visible = FALSE;
}

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;

	if ((type != LM_MESSAGE_SUB_TYPE_NOT_SET
	    && type != LM_MESSAGE_SUB_TYPE_HEADLINE
	    && type != LM_MESSAGE_SUB_TYPE_NORMAL
	    && type != LM_MESSAGE_SUB_TYPE_CHAT)
	    || server->ischannel(SERVER(server), from))
		return;
	node = lm_find_node(lmsg->node, "x", XMLNS, XMLNS_EVENT);
	if (node == NULL) {
		signal_emit("xmpp composing hide", 2, server, from);
		return;
	}
	if (lm_message_node_get_child(lmsg->node, "body") != NULL
	    || lm_message_node_get_child(lmsg->node, "subject") != NULL) {
		if (lm_message_node_get_child(node, "composing") != NULL)
			datalist_add(composings, server, from, g_strdup(id));
		else
			datalist_remove(composings, server, from);
		signal_emit("xmpp composing hide", 2, server, from);
	} else {
		if (lm_message_node_get_child(node, "composing") != NULL)
			signal_emit("xmpp composing show", 2, server, from);
		else
			signal_emit("xmpp composing hide", 2, server, from);
	}
}

static void
sig_send_message(XMPP_SERVER_REC *server, LmMessage *lmsg)
{
	LmMessageNode *node;
	LmMessageSubType type;

	type = lm_message_get_sub_type(lmsg);
	if ((type != LM_MESSAGE_SUB_TYPE_NOT_SET
	    && type != LM_MESSAGE_SUB_TYPE_HEADLINE
	    && type != LM_MESSAGE_SUB_TYPE_NORMAL
	    && type != LM_MESSAGE_SUB_TYPE_CHAT)
	    || (lm_message_node_get_child(lmsg->node, "body") == NULL
	    && lm_message_node_get_child(lmsg->node, "subject") == NULL))
		return;
	/* request composing events */
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_EVENT);
	lm_message_node_add_child(node, "composing", NULL);
}

static void
sig_offline(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	datalist_remove(composings, server, jid);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	if (IS_XMPP_SERVER(server))
		datalist_cleanup(composings, server);
}

static void
freedata_func(DATALIST_REC *rec)
{
	g_free(rec->data);
}

void
composing_init(void)
{
	composings = datalist_new(freedata_func);
	disco_add_feature(XMLNS_EVENT);
	signal_add("xmpp composing start", sig_composing_start);
	signal_add("xmpp composing stop", sig_composing_stop);
	signal_add("xmpp composing show", sig_composing_show);
	signal_add("xmpp composing hide", sig_composing_hide);
	signal_add("xmpp recv message", sig_recv_message);
	signal_add("xmpp send message", sig_send_message);
	signal_add("xmpp presence offline", sig_offline);
	signal_add("server disconnected", sig_disconnected);
}

void
composing_deinit(void)
{
	signal_remove("xmpp composing start", sig_composing_start);
	signal_remove("xmpp composing stop", sig_composing_stop);
	signal_remove("xmpp composing show", sig_composing_show);
	signal_remove("xmpp composing hide", sig_composing_hide);
	signal_remove("xmpp recv message", sig_recv_message);
	signal_remove("xmpp send message", sig_send_message);
	signal_remove("xmpp presence offline", sig_offline);
	signal_remove("server disconnected", sig_disconnected);
	datalist_destroy(composings);
}
