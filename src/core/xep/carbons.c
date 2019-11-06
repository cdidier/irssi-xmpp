/*
 * Copyright (C) 2017 Stephen Paul Weber
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
#include "misc.h"
#include "settings.h"
#include "signals.h"

#include "rosters-tools.h"
#include "xmpp-servers.h"
#include "disco.h"
#include "tools.h"
#include "muc.h"

#define XMLNS_CARBONS "urn:xmpp:carbons:2"

static void
sig_server_features(XMPP_SERVER_REC *server)
{
	if (disco_have_feature(server->server_features, XMLNS_CARBONS)) {
		LmMessage *lmsg;
		LmMessageNode *node;

		lmsg = lm_message_new_with_sub_type(
		    NULL,
		    LM_MESSAGE_TYPE_IQ,
		    LM_MESSAGE_SUB_TYPE_SET);
		node = lm_message_node_add_child(lmsg->node, "enable", NULL);
		lm_message_node_set_attribute(node, XMLNS, XMLNS_CARBONS);
		signal_emit("xmpp send iq", 2, server, lmsg);
		lm_message_unref(lmsg);
	}
}

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node = NULL;
	LmMessageNode *forwarded = NULL;
	LmMessageNode *message = NULL;
	char *ffrom, *fto;

	node = lm_find_node(lmsg->node, "received", "xmlns", XMLNS_CARBONS);
	if (node != NULL) {
		forwarded = lm_find_node(node, "forwarded", "xmlns", "urn:xmpp:forward:0");
		if (forwarded != NULL) {
			message = lm_find_node(forwarded, "message", "xmlns", "jabber:client");
		}
		if (message != NULL) {
			ffrom = xmpp_recode_in(lm_message_node_get_attribute(message, "from"));
			if (ffrom == NULL) {
				ffrom = g_strdup("");
			}
			fto = xmpp_recode_in(lm_message_node_get_attribute(message, "to"));
			if (fto == NULL) {
				fto = g_strdup("");
			}

			node = lmsg->node;
			lmsg->node = message;
			signal_emit("xmpp recv message", 6, server, lmsg, type, id, ffrom, fto);
			lmsg->node = node;
			goto done;
		}
	}

	node = lm_find_node(lmsg->node, "sent", "xmlns", XMLNS_CARBONS);
	if (node == NULL) {
		return; // Not for us
	}

	forwarded = lm_find_node(node, "forwarded", "xmlns", "urn:xmpp:forward:0");
	if (forwarded != NULL) {
		message = lm_find_node(forwarded, "message", "xmlns", "jabber:client");
	}
	if (message != NULL) {
		MUC_REC *channel;
		char *nick, *str;
		node = lm_message_node_get_child(message, "body");
		if (node == NULL || node->value == NULL || *node->value == '\0') {
			return;
		}
		str = xmpp_recode_in(node->value);

		ffrom = xmpp_recode_in(lm_message_node_get_attribute(message, "from"));
		if (ffrom == NULL) {
			ffrom = g_strdup("");
		}
		fto = xmpp_recode_in(lm_message_node_get_attribute(message, "to"));
		if (fto == NULL) {
			fto = g_strdup("");
		}

		nick = rosters_resolve_name(server, fto);
		if (nick != NULL) {
			g_free(fto);
			fto = nick;
		}

		if (type == LM_MESSAGE_SUB_TYPE_GROUPCHAT
		    && (channel = get_muc(server, fto)) != NULL
		    && (nick = muc_extract_nick(ffrom)) != NULL) {
			signal_emit("message xmpp carbons sent", 6, server,
			    str, nick, channel->name,
			    GINT_TO_POINTER(SEND_TARGET_CHANNEL));
			g_free(nick);
		} else if ((type == LM_MESSAGE_SUB_TYPE_NOT_SET
		           || type == LM_MESSAGE_SUB_TYPE_HEADLINE
		           || type == LM_MESSAGE_SUB_TYPE_NORMAL
		           || type == LM_MESSAGE_SUB_TYPE_CHAT)) {
			signal_emit("message xmpp carbons sent", 6, server,
			            str, ffrom, fto,
			            GINT_TO_POINTER(SEND_TARGET_NICK));
		}

		g_free(str);
	}

done:
	g_free(fto);
	g_free(ffrom);
	signal_stop();
}

void
carbons_init(void)
{
	signal_add("xmpp server features", sig_server_features);
	signal_add_first("xmpp recv message", sig_recv_message);
}

void
carbons_deinit(void)
{
	signal_remove("xmpp server features", sig_server_features);
	signal_remove("xmpp recv message", sig_recv_message);
}
