/*
 * $Id$
 *
 * Copyright (C) 2007 Colin DIDIER
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <sys/types.h>
#include <sys/utsname.h>
#include <string.h>
#include <time.h>

#include "module.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-protocol.h"
#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

#include "xmpp-xep.c"

void
xmpp_send_message(XMPP_SERVER_REC *server, const char *dest,
    const char *message)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *jid, *res, *jid_recoded, *message_recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);
	g_return_if_fail(message != NULL);

	jid = xmpp_rosters_resolve_name(server, dest);
	jid_recoded = xmpp_recode_out((jid != NULL) ? jid : dest);

	msg = lm_message_new_with_sub_type(jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(jid_recoded);

	message_recoded = xmpp_recode_out(message);
	lm_message_node_add_child(msg->node, "body", message_recoded);
	g_free(message_recoded);

	if (jid == NULL || !xmpp_have_resource(jid))
		goto send;

	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto send;

	res = xmpp_strip_resource(jid);
	resource = xmpp_rosters_find_resource(user, res);
	if (resource == NULL)
		goto send;
	g_free(res);
	
	/* stop composing */
	if (resource->composing_id != NULL) {
		child = lm_message_node_add_child(msg->node, "x", NULL);
		lm_message_node_set_attribute(child, XMLNS, XMLNS_EVENT);
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

send:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
}

static void
send_service_unavailable(XMPP_SERVER_REC *server, const char *dest,
    const char *id)
{
}

static void
sig_own_presence(XMPP_SERVER_REC *server, const int show, const char *status,
    const int priority)
{
	GSList *tmp;
	LmMessage *msg;
	const char *show_str, *status_tmp;
	char *status_recoded, *priority_str;

	g_return_if_fail(IS_XMPP_SERVER(server));

	status_tmp = server->away_reason;
	if (status == NULL && status_tmp != NULL
	    && strcmp(status_tmp, " ") == 0)
		status_tmp = NULL; 

	if (!xmpp_presence_changed(show, server->show, status,
	    status_tmp, priority, server->priority))
		return;

	switch (show) {
	case XMPP_PRESENCE_AWAY:
		show_str = xmpp_presence_show[XMPP_PRESENCE_AWAY];
		break;

	case XMPP_PRESENCE_CHAT:
		show_str = xmpp_presence_show[XMPP_PRESENCE_CHAT];
		break;

	case XMPP_PRESENCE_DND:
		show_str = xmpp_presence_show[XMPP_PRESENCE_DND];
		break;

	case XMPP_PRESENCE_XA:
		show_str = xmpp_presence_show[XMPP_PRESENCE_XA];
		break;
	
	case XMPP_PRESENCE_AVAILABLE:
	default:
		show_str = NULL;
	}

	/* away */
	if (show_str != NULL) {
		server->show = show;
		signal_emit("event 306", 2, server, server->jid);

	/* unaway */
	} else {
		server->show = XMPP_PRESENCE_AVAILABLE;
		if (server->usermode_away)
			signal_emit("event 305", 2, server, server->jid);
	}

	g_free(server->away_reason);
	server->away_reason = g_strdup(status);

	status_recoded = xmpp_recode_out(server->away_reason);
	if (!xmpp_priority_out_of_bound(priority))
		server->priority = priority;
	priority_str = g_strdup_printf("%d", server->priority);

	/* send presence to the server */
	msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);
	if (show_str != NULL)
		lm_message_node_add_child(msg->node, "show", show_str);
	if (status_recoded != NULL)
		lm_message_node_add_child(msg->node, "status", status_recoded);
	lm_message_node_add_child(msg->node, "priority", priority_str);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	/* send presence to channels */
	msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);
	if (show_str != NULL)
		lm_message_node_add_child(msg->node, "show", show_str);
	if (status_recoded != NULL)
		lm_message_node_add_child(msg->node, "status", status_recoded);

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		XMPP_CHANNEL_REC *channel;
		char *dest_recoded;
			
		channel = XMPP_CHANNEL(tmp->data);
		if (channel == NULL || !channel->joined)
			continue;

		dest_recoded = xmpp_recode_out(channel->name);
		lm_message_node_set_attribute(msg->node, "to", dest_recoded);
		g_free(dest_recoded);

		lm_send(server, msg, NULL);
	}

	lm_message_unref(msg);
	g_free(status_recoded);
	g_free(priority_str);

	if (server->usermode_away && server->away_reason == NULL)
		server->away_reason = g_strdup(" ");
}


/*
 * XEP-0203: Delayed Delivery
 * XEP-0091: Delayed Delivery (Obsolete)
 */

#define MAX_LEN_TIMESTAMP 255

static char *
get_timestamp(LmMessageNode *node)
{
	LmMessageNode *child;
	const char *stamp;

	/* XEP-0203: Delayed Delivery */
	child = lm_tools_message_node_find(node, "delay", XMLNS,
	    XMLNS_DELAYED_DELIVERY);
	if (child != NULL) {
		struct tm tm;

		stamp = lm_message_node_get_attribute(child, "stamp");
		if (stamp != NULL
		    && strptime(stamp, "%Y-%m-%dT%T", &tm) == NULL)
			return NULL;
		

		return NULL;
	}

	/* XEP-0091: Delayed Delivery (Obsolete) */
	child = lm_tools_message_node_find(node, "x", XMLNS,
	    XMLNS_DELAYED_DELIVERY_OLD);
	if (child != NULL) {
		struct tm tm;
		char str[MAX_LEN_TIMESTAMP];

		stamp = lm_message_node_get_attribute(child, "stamp");
		if (stamp != NULL
		    && strptime(stamp, "%Y%m%dT%T", &tm) == NULL)
			return NULL;

		if (strftime(str, MAX_LEN_TIMESTAMP,
		    settings_get_str("xmpp_timestamp_format"), &tm) == 0)
			return NULL;
		str[MAX_LEN_TIMESTAMP-1] = '\0';

		return g_strdup(str);
	}

	return NULL;
}


/*
 * Incoming messages handlers
 */

static LmHandlerResult
handle_message(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;
	LmMessageNode *child, *subchild;
	char *jid, *text, *stamp;

	server = XMPP_SERVER(user_data);
	if (server == NULL)
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	if (settings_get_bool("xmpp_raw_window")) {
		text = xmpp_recode_in(lm_message_node_to_string(msg->node));
		signal_emit("xmpp raw in", 2, server, text);
		g_free(text);
	}

	switch (lm_message_get_sub_type(msg)) {
	
	case LM_MESSAGE_SUB_TYPE_ERROR:

		/* MUC */
		text = xmpp_extract_channel(jid);
		channel = xmpp_channel_find(server, text);
		g_free(text);
		if (channel != NULL) {
			const char *code;

			child = lm_message_node_get_child(msg->node, "error");
			if (child == NULL)
				goto out;

			code = lm_message_node_get_attribute(child, "code");
			if (code == NULL)
				goto out;

			if (g_ascii_strcasecmp(code, "401") == 0)
				signal_emit("xmpp channel error", 2,
				    channel, "not allowed");

		/* general */
		} else {

			child = lm_message_node_get_child(msg->node, "body");
			if (child == NULL)
				signal_emit("message xmpp error", 2, server, jid);
			else {
				text = xmpp_recode_in(child->value);
				signal_emit("message xmpp error", 3, server, jid,
				    text);
				g_free(text);
			}
		}

		break;

	case LM_MESSAGE_SUB_TYPE_HEADLINE:
	case LM_MESSAGE_SUB_TYPE_NORMAL:
	case LM_MESSAGE_SUB_TYPE_CHAT:
		/* XEP-0022: Message Events */
		child = lm_message_node_get_child(msg->node, "x");
		if (child != NULL) {
			XMPP_QUERY_REC *query;

			subchild = lm_message_node_get_child(child,
			    "composing");
			
			query = xmpp_query_find(server, jid);
			if (query != NULL)
				query->composing_visible = (subchild != NULL);

			if (subchild != NULL)
				signal_emit("xmpp composing show", 2, server,
				    jid);
			else
				signal_emit("xmpp composing hide", 2, server,
				    jid);
		}

		stamp = get_timestamp(msg->node);

		/* <subject>text</subject> */
		child = lm_message_node_get_child(msg->node, "subject");
		if (child != NULL && child->value != NULL) {
			text = xmpp_recode_in(child->value);
			signal_emit("message private", 4, server, text,
			    jid, jid);
			g_free(text);
		}

		/* <body>text</body> */
		child = lm_message_node_get_child(msg->node, "body");
		if (child != NULL && child->value != NULL) {
			text = xmpp_recode_in(child->value);

			if (g_ascii_strncasecmp(text, "/me ", 4) == 0)
				signal_emit("message xmpp action", 5, server,
				    text+4, jid, jid,
				    GINT_TO_POINTER(SEND_TARGET_NICK));
			else
				signal_emit("message private", 4, server,
				    text, jid, jid);

			g_free(text);
		}

		g_free(stamp);
		break;

	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		/* <subject>text</subject> */
		child = lm_message_node_get_child(msg->node, "subject");
		if (child != NULL) {
			XMPP_CHANNEL_REC *channel;
			char *channel_name, *nick;

			channel_name = xmpp_extract_channel(jid);
			nick =  xmpp_extract_resource(jid);

			channel = xmpp_channel_find(server, channel_name);
			if (channel == NULL) {
				g_free(channel_name);
				g_free(nick);
				goto out;
			}

			text = xmpp_recode_in(child->value);
			signal_emit("xmpp channel topic", 3, channel, text,
			    nick);
			g_free(text);

			g_free(channel_name);
			g_free(nick);
		}
		
		/* <body>text</body> */
		child = lm_message_node_get_child(msg->node, "body");
		if (child != NULL && child->value != NULL) {
			XMPP_CHANNEL_REC *channel;
			char *channel_name, *nick;

			channel_name = xmpp_extract_channel(jid);
			nick =  xmpp_extract_resource(jid);
			stamp = get_timestamp(msg->node);

			/* it's my own message, so ignore it */
			channel = xmpp_channel_find(server, channel_name);
			if (stamp == NULL && (channel == NULL
			    || nick == NULL || (channel != NULL
			    && strcmp(nick, channel->nick) == 0))) {
				g_free(channel_name);
				g_free(nick);
				goto out;
			}

			text = xmpp_recode_in(child->value);

			if (g_ascii_strncasecmp(text, "/me ", 4) == 0)
				signal_emit("message xmpp action", 5, server,
				    text+4, nick, channel_name,
				    GINT_TO_POINTER(SEND_TARGET_CHANNEL));
			else
				signal_emit("message public", 5, server,
				    text, nick, "", channel_name);

			g_free(text);
			g_free(stamp);
			g_free(channel_name);
			g_free(nick);
		}

		break;

	default:
		break;
	}

out:
	g_free(jid);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
handle_presence(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;
	LmMessageNode *child, *subchild, *show, *priority;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
	if (server == NULL)
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	if (settings_get_bool("xmpp_raw_window")) {
		text = xmpp_recode_in(lm_message_node_to_string(msg->node));
		signal_emit("xmpp raw in", 2, server, text);
		g_free(text);
	}

	switch (lm_message_get_sub_type(msg)) {

	case LM_MESSAGE_SUB_TYPE_ERROR:

		/* MUC */
		text = xmpp_extract_channel(jid);
		channel = xmpp_channel_find(server, text);
		g_free(text);
		if (channel != NULL) {
			const char *code;
			char *nick;

			/* <error code='code'/> */
			child = lm_message_node_get_child(msg->node, "error");
			if (child == NULL)
				goto out;
			code = lm_message_node_get_attribute(child, "code");
			if (code == NULL)
				goto out;

			nick = xmpp_extract_nick(jid);

			if (!channel->joined) {
				/* <error code='401'/> */
				if (g_ascii_strcasecmp(code, "401") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_PASSWORD_INVALID_OR_MISSING);
				/* <error code='403'/> */
				else if (g_ascii_strcasecmp(code, "403") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_USER_BANNED);
				/* <error code='404'/> */
				else if (g_ascii_strcasecmp(code, "404") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_ROOM_NOT_FOUND);
				/* <error code='405'/> */
				else if (g_ascii_strcasecmp(code, "405") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_ROOM_CREATION_RESTRICTED);
				/* <error code='406'/> */
				else if (g_ascii_strcasecmp(code, "406") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_USE_RESERVED_ROOM_NICK);
				/* <error code='407'/> */
				else if (g_ascii_strcasecmp(code, "407") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_NOT_ON_MEMBERS_LIST);
				/* <error code='409'/> */
				else if (g_ascii_strcasecmp(code, "409") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_NICK_IN_USE);
				/* <error code='503'/> */
				else if (g_ascii_strcasecmp(code, "503") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_MAXIMUM_USERS_REACHED);
			} else {
				/* <error code='409'/> */
				if (g_ascii_strcasecmp(code, "409") == 0)
					signal_emit("message xmpp channel nick in use",
					    2, channel, nick);
			}

			g_free(nick);

		/* general */
		} else
			signal_emit("xmpp presence error", 2, server, jid);

		break;

	case LM_MESSAGE_SUB_TYPE_SUBSCRIBE:
		/* <status>text</status> */
		child = lm_message_node_get_child(msg->node, "status");
		text = (child != NULL) ? xmpp_recode_in(child->value) : NULL;
		signal_emit("xmpp presence subscribe", 3, server, jid, text);
		g_free(text);
		break;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE:
		signal_emit("xmpp presence unsubscribe", 2, server, jid);
		break;

	case LM_MESSAGE_SUB_TYPE_SUBSCRIBED:
		signal_emit("xmpp presence subscribed", 2, server, jid);
		break;

	case LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED:
		signal_emit("xmpp presence unsubscribed", 2, server, jid);
		break;

	case LM_MESSAGE_SUB_TYPE_AVAILABLE:

		/* MUC */
		text = xmpp_extract_channel(jid);
		channel = xmpp_channel_find(server, text);
		g_free(text);
		if (channel != NULL) {
			const char *item_affiliation, *item_role;
			char *nick, *item_jid, *item_nick;
			gboolean own, forced, created;

			nick = xmpp_extract_nick(jid);
			item_affiliation = item_role = NULL;
			item_jid = item_nick = NULL;
			own = forced = created = FALSE;

			/* <x xmlns='http://jabber.org/protocol/muc#user'/> */
			child = lm_tools_message_node_find(msg->node, "x",
			    XMLNS, XMLNS_MUC_USER);
			if (child != NULL) {

				if ((subchild = lm_message_node_get_child(
				    child, "item")) != NULL) {
					/* <item affiliation='item_affiliation'
					 *     role='item_role'
					 *     nick='item_nick'/> */
					item_affiliation =
					    lm_message_node_get_attribute(
					    subchild, "affiliation");
					item_role =
					    lm_message_node_get_attribute(
					    subchild, "role");
					item_jid = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    subchild, "jid"));
					item_nick = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    subchild, "nick"));

					/* <status code='110'/> */
					own = lm_tools_message_node_find(
					    child, "status", "code", "110")
					    != NULL ? TRUE : FALSE;
					/* <status code='210'/> */
					forced = lm_tools_message_node_find(
					    child, "status", "code", "210")
					    != NULL ? TRUE : FALSE;
					/* <status code='201'/> */
					created = lm_tools_message_node_find(
					    child, "status", "code", "201")
					    != NULL ? TRUE : FALSE;
				}
			}

			if (item_nick != NULL) {
				g_free(nick);
				nick = item_nick;
			}

			if (created) {
				/* TODO send disco
				 * show event IRCTXT_CHANNEL_CREATED */
			}

			if (own || strcmp(nick, channel->nick) == 0)
				signal_emit("xmpp channel nick own_event", 6,
				    channel, nick, item_jid, item_affiliation,
				    item_role, forced);
			else
				signal_emit("xmpp channel nick event", 5,
				    channel, nick, item_jid, item_affiliation,
				    item_role);

			/* <status>text</status> */
			child = lm_message_node_get_child(msg->node, "status");
			text = child != NULL ?
			    xmpp_recode_in(child->value) : NULL;
			/* <show>show</show> */
			 show = lm_message_node_get_child(msg->node, "show");

			signal_emit("xmpp channel nick presence", 4, channel,
			    nick, (show != NULL) ? show->value : NULL, text);

			g_free(item_jid);
			g_free(text);
			g_free(nick);

		/* general */
		} else {
			/* <status>text</status> */
			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;
			/* <show>show</show> */
			show = lm_message_node_get_child(msg->node, "show");
			/* <priority>priority</priority> */
			priority = lm_message_node_get_child(msg->node,
			    "priority");

			signal_emit("xmpp presence update", 5, server, jid,
			    (show != NULL) ? show->value : NULL, text,
			    (priority != NULL) ? priority->value : NULL);

			g_free(text);
		}

		break;

	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:

		/* MUC */
		text = xmpp_extract_channel(jid);
		channel = xmpp_channel_find(server, text);
		g_free(text);
		if (channel != NULL) {
			const char *status_code;
			char *nick, *reason, *actor, *item_nick;

			nick = xmpp_extract_nick(jid);
			status_code = NULL;
			reason = actor = item_nick = NULL;

			/* <x xmlns='http://jabber.org/protocol/muc#user'/> */
			child = lm_tools_message_node_find(msg->node, "x",
			    XMLNS, XMLNS_MUC_USER);
			if (child != NULL) {

				/* <status code='status_code'/> */
				if ((subchild = lm_message_node_get_child(
				    child, "status")) != NULL)
					status_code =
					    lm_message_node_get_attribute(
					    subchild, "code");

				/* <item nick='item_nick'/> */
				if ((child = lm_message_node_get_child(
				    child, "item")) != NULL) {
					item_nick = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    child, "nick"));

					/* <reason>reason</reason>*/
					if ((subchild =
					    lm_message_node_get_child(child,
					    "reason")) != NULL)
						reason = xmpp_recode_in(
						    subchild->value);

					/* <actor jid='actor'/> */
					if ((subchild =
					    lm_message_node_get_child(child,
					    "actor")) != NULL)
						actor = xmpp_recode_in(
					    	    lm_message_node_get_attribute(
						    subchild, "jid"));
				}
			}
			
			if (status_code != NULL) {

				/* <status code='303'/> */
			 	if (g_ascii_strcasecmp(status_code,
				    "303") == 0 && item_nick != NULL)
					signal_emit("xmpp channel nick",
					    5, channel, nick, item_nick);

				/* <status code='307'/> */
				else if (g_ascii_strcasecmp(status_code,
				    "307") == 0)
					signal_emit("xmpp channel nick kicked",
					    4, channel, nick, actor, reason);
					
				/* ban: <status code='301'/> */
				else if (g_ascii_strcasecmp(status_code,
				    "301") == 0)
					signal_emit("xmpp channel nick kicked",
					    4, channel, nick, actor, reason);

			} else {
				/* <status>text</status> */
				child = lm_message_node_get_child(msg->node,
				    "status");
				text = (child != NULL) ?
				    xmpp_recode_in(child->value) : NULL;

				signal_emit("xmpp channel nick part", 3,
				    channel, nick, text);

				g_free(text);
			}

			g_free(item_nick);
			g_free(reason);
			g_free(actor);
			g_free(nick);

		/* general */
		} else {
			/* <status>text</status> */
			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;

			signal_emit("xmpp presence unavailable", 3, server,
			    jid, text);

			g_free(text);
		}

		break;

	default:
		break;
	}

out:
	g_free(jid);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
handle_iq(LmMessageHandler *handler, LmConnection *connection,
	LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;
	LmMessageNode *child;
	const char *xmlns;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
	if (server == NULL)
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;

	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	if (settings_get_bool("xmpp_raw_window")) {
		text = xmpp_recode_in(lm_message_node_to_string(msg->node));
		signal_emit("xmpp raw in", 2, server, text);
		g_free(text);
	}

	child = lm_message_node_get_child(msg->node, "query");
	if (child == NULL)
		child = lm_message_node_get_child(msg->node, "vCard");

	xmlns = (child != NULL) ?
	    lm_message_node_get_attribute(child, XMLNS) : NULL;

	switch (lm_message_get_sub_type(msg)) {

	case LM_MESSAGE_SUB_TYPE_ERROR:

		/* MUC */
		text = xmpp_extract_channel(jid);
		channel = xmpp_channel_find(server, text);
		g_free(text);
		if (channel != NULL) {
			const char *code;

			child = lm_message_node_get_child(msg->node, "error");
			if (child == NULL)
				goto out;

			code = lm_message_node_get_attribute(child, "code");
			if (code == NULL)
				goto out;



		} 

		break;

	case LM_MESSAGE_SUB_TYPE_GET:
		/* XEP-0092: Software Version */
		if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_VERSION) == 0)
			version_send(server, jid,
			    lm_message_node_get_attribute(msg->node, "id"));

		/* service-unavailable */
		else {
		/* <service-unavailable/> */
		}

		break;

	case LM_MESSAGE_SUB_TYPE_RESULT:
		if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_ROSTER) == 0)
			signal_emit("xmpp roster update", 2, server, child);

		else if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_VERSION) == 0)
			version_handle(server, jid, child);

		else if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_VCARD) == 0)
			vcard_handle(server, jid, child);

		else if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_DISCO_INFO) == 0) {
			XMPP_CHANNEL_REC *channel;

			/* MUC */
			text = xmpp_extract_channel(jid);
			channel = xmpp_channel_find(server, text);
			g_free(text);
			if (channel != NULL)
				signal_emit("xmpp channel disco", 2, channel,
				    child);
			
			/* server */
			else if (strcmp(jid, server->host) == 0)
				signal_emit("xmpp server disco", 2, server,
				    child);

		} else
			signal_emit("xmpp ping handle", 3, server, jid,
			    lm_message_node_get_attribute(msg->node, "id"));
		
		break;

	case LM_MESSAGE_SUB_TYPE_SET :
		if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_ROSTER) == 0)
			signal_emit("xmpp roster update", 2, server, child);
		break;
	
	default:
		break;
	}

out:
	g_free(jid);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
unregister_handlers(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	/* unregister handlers */
	if (server->hmessage != NULL) {
		if (lm_message_handler_is_valid(server->hmessage))
			lm_message_handler_invalidate(server->hmessage);
		lm_message_handler_unref(server->hmessage);
		server->hmessage = NULL;
	}

	if (server->hpresence != NULL) {
		if (lm_message_handler_is_valid(server->hpresence))
			lm_message_handler_invalidate(server->hpresence);
		lm_message_handler_unref(server->hpresence);
		server->hpresence = NULL;
	}

	if (server->hiq != NULL) {
		if (lm_message_handler_is_valid(server->hiq))
			lm_message_handler_invalidate(server->hiq);
		lm_message_handler_unref(server->hiq);
		server->hiq = NULL;
	}
}

static void
register_handlers(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	if (server->hmessage != NULL || server->hpresence != NULL ||
	    server->hiq != NULL)
		unregister_handlers(server);

	/* handle message */
	server->hmessage = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_message, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn,
	    server->hmessage, LM_MESSAGE_TYPE_MESSAGE,
	    LM_HANDLER_PRIORITY_NORMAL);

	/* handle presence */
	server->hpresence = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_presence, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn,
	    server->hpresence, LM_MESSAGE_TYPE_PRESENCE,
	    LM_HANDLER_PRIORITY_NORMAL);

	/* handle iq */
	server->hiq = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_iq, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn,
	    server->hiq, LM_MESSAGE_TYPE_IQ,
	    LM_HANDLER_PRIORITY_NORMAL);
}

void
xmpp_protocol_init(void)
{
	signal_add_first("server connected",
	    (SIGNAL_FUNC)register_handlers);
	signal_add_first("server disconnected",
	    (SIGNAL_FUNC)unregister_handlers);
	signal_add_first("xmpp own_presence", (SIGNAL_FUNC)sig_own_presence);
	signal_add_first("xmpp server disco",
	    (SIGNAL_FUNC)disco_servers_services);
	signal_add("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_add("xmpp composing stop", (SIGNAL_FUNC)composing_stop);

	settings_add_int("xmpp", "xmpp_priority", 0);
	settings_add_bool("xmpp", "xmpp_send_version", TRUE);
	settings_add_bool("xmpp_lookandfeel", "xmpp_raw_window", FALSE);
	settings_add_str("xmpp_lookandfeel", "xmpp_timestamp_format",
	    "%Y-%m-%d %H:%M");
}

void
xmpp_protocol_deinit(void)
{
	signal_remove("server connected",
	    (SIGNAL_FUNC)register_handlers);
	signal_remove("server disconnected",
	    (SIGNAL_FUNC)register_handlers);
	signal_remove("xmpp own_presence", (SIGNAL_FUNC)sig_own_presence);
	signal_remove("xmpp server disco", (SIGNAL_FUNC)disco_servers_services);
	signal_remove("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_remove("xmpp composing stop", (SIGNAL_FUNC)composing_stop);
}
