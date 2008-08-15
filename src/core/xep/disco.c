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

 /* XEP-0030: Service Discovery */

#include <string.h>

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "tools.h"

#define XMLNS_DISCO "http://jabber.org/protocol/disco#info"

void
xmpp_add_feature(XMPP_SERVER_REC *server, const char *feature)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(feature != NULL && *feature == '\0');
	server->my_features = g_slist_prepend(server->my_features,
	    g_strdup(feature));
}

static void
cleanup_features(GSList *list)
{
	GSList *tmp, *next;

	for (tmp = list; tmp != NULL; tmp = next) {
		next = tmp->next;
		g_free(tmp->data);
		list = g_slist_remove(list, tmp->data);
	}
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;
	GSList *features;

	if (type != LM_MESSAGE_SUB_TYPE_RESULT)
		return;
	node = lm_find_node(lmsg->node, "query", "xmlns", XMLNS_DISCO);
	if (node == NULL)
		return;
	features = NULL;
	for (node = node->children; node != NULL; node = node->next) {
		if (strcmp(node->name, "feature") == 0) {
			features = g_slist_prepend(features, xmpp_recode_in(
			    lm_message_node_get_attribute(node, "var")));
		}
	}
	signal_emit("xmpp features", 3, server, from, features);
	if (strcmp(from, server->host) == 0) {
		cleanup_features(server->server_features);
		server->server_features = features;
		signal_emit("xmpp server features", 1, server);
	} else
		cleanup_features(features);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	LmMessage *lmsg;
	LmMessageNode *node;

	g_return_if_fail(IS_XMPP_SERVER(server));
	/* discover server's features */
	lmsg = lm_message_new_with_sub_type(server->host, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_DISCO);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	cleanup_features(server->server_features);
	server->server_features = NULL;
	cleanup_features(server->my_features);
	server->my_features = NULL;
}

void
disco_init(void)
{
	signal_add("server connected", sig_connected);
	signal_add("server disconnected", sig_disconnected);
	signal_add("xmpp recv iq", sig_recv_iq);
}

void
disco_deinit(void)
{
	signal_remove("server connected", sig_connected);
	signal_add("server disconnected", sig_disconnected);
	signal_remove("xmpp recv iq", sig_recv_iq);
}
