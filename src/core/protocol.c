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

#include "module.h"
#include "channels.h"
#include "signals.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "tools.h"

static void
sig_set_presence(XMPP_SERVER_REC *server, const int show, const char *status,
    const int priority)
{
	LmMessage *lmsg;
	char *str;

	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!xmpp_presence_changed(show, server->show, status,
	    server->away_reason, priority, server->priority)) {
		signal_stop();
		return;
	}
	server->show = show;
	g_free(server->away_reason);
	server->away_reason = g_strdup(status);
	if (!xmpp_priority_out_of_bound(priority))
		server->priority = priority;
	lmsg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);
	if (show != XMPP_PRESENCE_AVAILABLE)
		lm_message_node_add_child(lmsg->node, "show",
		    xmpp_presence_show[server->show]);
	if (status != NULL) {
		str = xmpp_recode_out(server->away_reason);
		lm_message_node_add_child(lmsg->node, "status", str);
		g_free(str);
	}
	str = g_strdup_printf("%d", server->priority);
	lm_message_node_add_child(lmsg->node, "priority", str);
	g_free(str);
	signal_emit("xmpp send presence", 2, server, lmsg);
	lm_message_unref(lmsg);
	if (show != XMPP_PRESENCE_AVAILABLE) /* away */
		signal_emit("event 306", 2, server, server->jid);
	else if (server->usermode_away) /* unaway */
		signal_emit("event 305", 2, server, server->jid);
}

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;
	char *str, *subject;
	const char *from_stripped;

	/* assing from_stripped. If setting xmpp_strip_resource is
	   True and from is not from a channel private message set
	   from_stripped to same as from, but strip the
	   resource. Otherwise just duplicate from to
	   from_stripped. */
	str = xmpp_strip_resource(from);
	from_stripped =
		((settings_get_bool("xmpp_strip_resource")) &&
		 (channel_find(SERVER(server), str)) == NULL) ?
		g_strdup(str) : g_strdup(from);
	g_free(str);
	
	if ((type != LM_MESSAGE_SUB_TYPE_NOT_SET
	    && type != LM_MESSAGE_SUB_TYPE_HEADLINE
	    && type != LM_MESSAGE_SUB_TYPE_NORMAL
	    && type != LM_MESSAGE_SUB_TYPE_CHAT)
	    || server->ischannel(SERVER(server), from))
		return;
	node = lm_message_node_get_child(lmsg->node, "subject");
	if (node != NULL && node->value != NULL && *node->value != '\0') {
		str = xmpp_recode_in(node->value);
		subject = g_strconcat("Subject: ", str, (void *)NULL);
		g_free(str);
		signal_emit("message private", 4, server, subject, from_stripped, from);
		g_free(subject);
	}
	node = lm_message_node_get_child(lmsg->node, "body");
	if (node != NULL && node->value != NULL && *node->value != '\0') {
		str = xmpp_recode_in(node->value);
		if (g_ascii_strncasecmp(str, "/me ", 4) == 0)
			signal_emit("message xmpp action", 5,
			    server, str+4, from_stripped, from,
			    GINT_TO_POINTER(SEND_TARGET_NICK));
		else
			signal_emit("message private", 4, server,
			    str, from_stripped, from);
		g_free(str);
	}
}

void
protocol_init(void)
{
	signal_add_first("xmpp set presence", sig_set_presence);
	signal_add("xmpp recv message", sig_recv_message);
}

void
protocol_deinit(void)
{
	signal_remove("xmpp set presence", sig_set_presence);
	signal_remove("xmpp recv message", sig_recv_message);
}
