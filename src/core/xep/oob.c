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
 * XEP-0066: Out of Band Data
 */

#include "module.h"
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "tools.h"
#include "disco.h"
#include "muc.h"

#define XMLNS_OOB_X	"jabber:x:oob"
#define XMLNS_OOB_IQ	"jabber:iq:oob"

static void
lm_message_node_delete_child(LmMessageNode *child)
{
	LmMessageNode *n;
	LmMessageNode *parent = child->parent;
	child->parent = NULL;
	for (n = parent->children; n; ) {
		LmMessageNode *next = n->next;
		if (n == child) {
			LmMessageNode *prev = n->prev;
			if (next != NULL) {
				next->prev = prev;
			}
			if (prev != NULL) {
				prev->next = next;
			}
			if (n == parent->children) {
				parent->children = next;
			}
			n->prev = NULL;
			n->next = NULL;
			lm_message_node_unref(n);
		}
		n = next;
	}
}

static void
sig_recv_x(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node, *child;
	const char *url, *desc;
	char *url_recoded, *desc_recoded, *str;

	node = lm_find_node(lmsg->node, "x", XMLNS, XMLNS_OOB_X);
	if (node != NULL) {
		child = lm_message_node_get_child(node, "url");
		if (child == NULL || child->value == NULL)
			return;
		url = child->value;
		child = lm_message_node_get_child(node, "desc");
		desc = child != NULL ? child->value : NULL;
		if (lm_message_get_type(lmsg) == LM_MESSAGE_TYPE_MESSAGE) {
			LmMessageNode *body = lm_message_node_get_child(lmsg->node, "body");
			if (body != NULL && g_strcmp0(url, lm_message_node_get_value(body)) == 0) {
				lm_message_node_delete_child(body);
			}
		}
		url_recoded = xmpp_recode_in(url);
		if (desc != NULL) {
			desc_recoded = xmpp_recode_in(desc);
			str = g_strconcat(desc_recoded, ": ", url_recoded, (void *)NULL);
			g_free(url_recoded);
			g_free(desc_recoded);
		} else
			str = url_recoded;
		if (lm_message_get_sub_type(lmsg) == LM_MESSAGE_SUB_TYPE_GROUPCHAT) {
			char *nick, *channel;

			nick = muc_extract_nick(from);
			channel = muc_extract_channel(from);

			signal_emit("message public", 5,
				    server, str, nick, "", channel);

			g_free(channel);
			g_free(nick);
		} else {
			signal_emit("message private", 4, server, str, from, from);
		}
		g_free(str);
	}
}

void
oob_init(void)
{
	disco_add_feature(XMLNS_OOB_X);
	signal_add("xmpp recv message", sig_recv_x);
	signal_add("xmpp recv presence", sig_recv_x);
}

void
oob_deinit(void)
{
	signal_remove("xmpp recv message", sig_recv_x);
	signal_remove("xmpp recv presence", sig_recv_x);
}
