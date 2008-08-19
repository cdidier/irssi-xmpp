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

/*
 * XEP-0022: Message Events
 */

#include <string.h>

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "xmpp-queries.h"
#include "rosters-tools.h"
#include "tools.h"
#include "disco.h"

#define XMLNS_EVENT "jabber:x:event"

struct composing_data {
	XMPP_SERVER_REC *server;
	char		*jid;
	char		*id;
};

static GSList *composings;

static struct composing_data *
find_cd(XMPP_SERVER_REC *server, const char *jid)
{
	GSList *tmp;
	struct composing_data *cd;

	for (tmp = composings; tmp != NULL; tmp = tmp->next) {
		cd = tmp->data;
		if (cd->server == server && strcmp(cd->jid, jid) == 0)
			return cd;
	}
	return NULL;
}

static void
add_cd(XMPP_SERVER_REC *server, const char *jid, const char *id)
{
	struct composing_data *cd;

	if ((cd = find_cd(server, jid)) != NULL) {
		g_free(cd->id);
		cd->id = g_strdup(id);
	} else {
		cd = g_new0(struct composing_data, 1);
		cd->server = server;
		cd->jid = g_strdup(jid);
		cd->id = g_strdup(id);
		composings = g_slist_append(composings, cd);
	}
}

static void
cleanup_cd(struct composing_data *cd)
{
	composings = g_slist_remove(composings, cd);
	g_free(cd->jid);
	g_free(cd->id);
	g_free(cd);
}

static void
remove_cd(XMPP_SERVER_REC *server, const char *jid)
{
	struct composing_data *cd;

	if ((cd = find_cd(server, jid)) != NULL)
		cleanup_cd(cd);
}

static void
cleanup_composings(XMPP_SERVER_REC *server)
{
	GSList *tmp, *next;
	struct composing_data *cd;

	for (tmp = composings; tmp != NULL; tmp = next) {
		next = tmp->next;
		cd = tmp->data;
		if (server == NULL || cd->server == server)
			cleanup_cd(cd);
	}
}

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
	lm_message_node_set_attribute(node, "xmlns", XMLNS_EVENT);
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
	struct composing_data *cd;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);
	if ((cd = find_cd(server, dest)) != NULL)
		send_start(server, dest, cd->id);
}

static void
sig_composing_stop(XMPP_SERVER_REC *server, const char *dest)
{
	struct composing_data *cd;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);
	if ((cd = find_cd(server, dest)) != NULL)
		send_stop(server, dest, cd->id);
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

	if (type != LM_MESSAGE_SUB_TYPE_NOT_SET
	    && type != LM_MESSAGE_SUB_TYPE_HEADLINE
	    && type != LM_MESSAGE_SUB_TYPE_NORMAL
	    && type != LM_MESSAGE_SUB_TYPE_CHAT)
		return;
	node = lm_find_node(lmsg->node, "x", "xmlns", XMLNS_EVENT);
	if (node == NULL) {
		remove_cd(server, from);
		signal_emit("xmpp composing hide", 2, server, from);
		return;
	}
	if (lm_message_node_get_child(lmsg->node, "body") != NULL
	    || lm_message_node_get_child(lmsg->node, "subject") != NULL) {
		if (lm_message_node_get_child(node, "composing") != NULL)
			add_cd(server, from, id);
		else
			remove_cd(server, from);
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
	lm_message_node_set_attribute(node, "xmlns", XMLNS_EVENT);
	lm_message_node_add_child(node, "composing", NULL);
}

static void
sig_offline(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	remove_cd(server, jid);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;
	cleanup_composings(server);
}


void
composing_init(void)
{
	composings = NULL;
	xmpp_add_feature(XMLNS_EVENT);

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

	cleanup_composings(NULL);
}
