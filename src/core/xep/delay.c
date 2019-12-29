/*
 * Copyright (C) 2009 Colin DIDIER
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
 * XEP-0203: Delayed Delivery
 */

#include "module.h"
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "tools.h"
#include "datetime.h"
#include "disco.h"
#include "muc.h"

#define XMLNS_DELAY	"urn:xmpp:delay"
#define XMLNS_OLD_DELAY	"jabber:x:delay"

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;
	MUC_REC *channel;
	const char *stamp;
	char *nick, *str;
	time_t t;

	node = lm_find_node(lmsg->node, "delay", "xmlns", XMLNS_DELAY);
	if (node == NULL) {
		/* XEP-0091: Delayed Delivery (deprecated) */
		node = lm_find_node(lmsg->node, "x", "xmlns", XMLNS_OLD_DELAY);
		if (node == NULL)
			return;
	}
	stamp = lm_message_node_get_attribute(node, "stamp");
	if ((t = xep82_datetime(stamp)) == (time_t)-1)
		return;
	node = lm_message_node_get_child(lmsg->node, "body");
	if (node == NULL || node->value == NULL || *node->value == '\0')
		return;
	if (type == LM_MESSAGE_SUB_TYPE_GROUPCHAT
	    && (channel = get_muc(server, from)) != NULL
	    && (nick = muc_extract_nick(from)) != NULL) {
		str = xmpp_recode_in(node->value);
		if (g_ascii_strncasecmp(str, "/me ", 4) == 0)
			 signal_emit("message xmpp delay action", 6,
			     server, str+4, nick, channel->name, &t,
			     GINT_TO_POINTER(SEND_TARGET_CHANNEL));
		else
			 signal_emit("message xmpp delay", 6, server,
			     str, nick, channel->name, &t,
			     GINT_TO_POINTER(SEND_TARGET_CHANNEL));
		g_free(str);
		g_free(nick);
	} else if ((type == LM_MESSAGE_SUB_TYPE_NOT_SET
	    || type == LM_MESSAGE_SUB_TYPE_HEADLINE
	    || type == LM_MESSAGE_SUB_TYPE_NORMAL
	    || type == LM_MESSAGE_SUB_TYPE_CHAT)) {
		str = xmpp_recode_in(node->value);
		if (g_ascii_strncasecmp(str, "/me ", 4) == 0)
			signal_emit("message xmpp delay action", 6,
			    server, str+4, from, from, &t,
			    GINT_TO_POINTER(SEND_TARGET_NICK));
		else
			signal_emit("message xmpp delay", 6, server,
			    str, from, from, &t,
			    GINT_TO_POINTER(SEND_TARGET_NICK));
		g_free(str);
	} else
		return;
	signal_stop();
}

void
delay_init(void)
{
	disco_add_feature(XMLNS_DELAY);
	signal_add_first("xmpp recv message", sig_recv_message);
}

void
delay_deinit(void)
{
	signal_remove("xmpp recv message", sig_recv_message);
}
