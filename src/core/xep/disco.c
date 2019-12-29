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

 /* XEP-0030: Service Discovery */

#include <string.h>

#include "module.h"
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "tools.h"
#include "disco.h"

#define XMLNS_DISCO "http://jabber.org/protocol/disco#info"

static GSList *my_features;

void
disco_add_feature(char *feature)
{
	g_return_if_fail(feature != NULL && *feature != '\0');
	my_features = g_slist_insert_sorted(my_features, feature,
	    (GCompareFunc)strcmp);
}

gboolean
disco_have_feature(GSList *list, const char *feature)
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
	g_slist_free_full(list, g_free);
}

void
disco_request(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL && *dest != '\0');
	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_DISCO);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
send_disco(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node, *child;
	GSList *tmp;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_DISCO);
	child = lm_message_node_add_child(node, "identity", NULL);
	lm_message_node_set_attribute(child, "category", "client");
	lm_message_node_set_attribute(child, "type", "console");
	lm_message_node_set_attribute(child, "name", IRSSI_XMPP_PACKAGE);
	for (tmp = my_features; tmp != NULL; tmp = tmp->next) {
		child = lm_message_node_add_child(node, "feature", NULL);
		lm_message_node_set_attribute(child, "var", tmp->data);
	}
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;
	GSList *features;

	if (type == LM_MESSAGE_SUB_TYPE_RESULT) {
		node = lm_find_node(lmsg->node, "query", XMLNS, XMLNS_DISCO);
		if (node == NULL)
			return;
		features = NULL;
		for (node = node->children; node != NULL; node = node->next) {
			if (strcmp(node->name, "feature") == 0) {
				features = g_slist_prepend(features,
				    xmpp_recode_in(
			    	    lm_message_node_get_attribute(node, "var")));
			}
		}
		signal_emit("xmpp features", 3, server, from, features);
		if (strcmp(from, server->domain) == 0) {
			cleanup_features(server->server_features);
			server->server_features = features;
			signal_emit("xmpp server features", 1, server);
		} else
			cleanup_features(features);
	} else if (type == LM_MESSAGE_SUB_TYPE_GET) {
		node = lm_find_node(lmsg->node, "query", XMLNS, XMLNS_DISCO);
		if (node != NULL)
			send_disco(server, from);
	}
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	if (IS_XMPP_SERVER(server))
		disco_request(server, server->domain);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;
	cleanup_features(server->server_features);
	server->server_features = NULL;
}

static void
sig_disco_add_feature(const char *feature)
{
	disco_add_feature(g_strdup(feature));
}

void
disco_init(void)
{
	my_features = NULL;
	disco_add_feature(XMLNS_DISCO);
	signal_add("server connected", sig_connected);
	signal_add("server disconnected", sig_disconnected);
	signal_add("xmpp recv iq", sig_recv_iq);
	signal_add("xmpp register feature", sig_disco_add_feature);
}

void
disco_deinit(void)
{
	signal_remove("server connected", sig_connected);
	signal_remove("server disconnected", sig_disconnected);
	signal_remove("xmpp recv iq", sig_recv_iq);
	g_slist_free(my_features);
}
