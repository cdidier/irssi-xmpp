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

static GSList *my_features;

void
xmpp_add_feature(char *feature)
{
	g_return_if_fail(feature != NULL && *feature != '\0');
	my_features = g_slist_prepend(my_features, feature);
}

gboolean
xmpp_have_feature(GSList *list, const char *feature)
{
	GSList *tmp;

	for (tmp = list; tmp != NULL; tmp = tmp->next)
		if (strcmp(feature, tmp->data) == 0)
			return TRUE;
	return FALSE;
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
send_disco(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_DISCO);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	send_disco(server, server->host);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	cleanup_features(server->server_features);
	server->server_features = NULL;
}

void
disco_init(void)
{
	my_features = NULL;
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
	g_slist_free(my_features);
}
