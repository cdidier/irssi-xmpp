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

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "tools.h"

#define XMLNS_EVENT "jabber:x:event"

static void
send_start(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *recoded;
	const char *id;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_EVENT);
	lm_message_node_add_child(node, "composing", NULL);
	rosters_find_user(server->roster, dest, NULL, &resource);
	if (resource != NULL) {
		id = lm_message_node_get_attribute(lmsg->node, "id");
		lm_message_node_add_child(node, "id", id);
		g_free_and_null(resource->composing_id);
		resource->composing_id = g_strdup(id);
	}
	signal_emit("xmpp send message", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
send_stop(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_EVENT);
	rosters_find_user(server->roster, dest, NULL, &resource);
	if (resource != NULL && resource->composing_id != NULL) {
		lm_message_node_add_child(node, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}
	signal_emit("xmpp send message", 2, server, lmsg);
	lm_message_unref(lmsg);
}

void
composing_init(void)
{
}

void
composing_deinit(void)
{
}
