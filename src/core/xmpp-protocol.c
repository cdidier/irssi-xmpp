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

#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>

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

void
xmpp_send_message(XMPP_SERVER_REC *server, const char *to_jid,
    const char *message)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *jid, *res, *to_full_jid, *to_jid_recoded, *message_recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(to_jid != NULL);
	g_return_if_fail(message != NULL);

	to_full_jid = xmpp_rosters_get_full_jid(server->roster, to_jid);
	to_jid_recoded =
	    xmpp_recode_out((to_full_jid != NULL) ? to_full_jid : to_jid);
	g_free(to_full_jid);

	message_recoded = xmpp_recode_out(message);

	msg = lm_message_new_with_sub_type(to_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(to_jid_recoded);

	lm_message_node_add_child(msg->node, "body", message_recoded);
	g_free(message_recoded);

	jid = xmpp_strip_resource(to_jid);
	res = xmpp_extract_resource(to_jid);

	if (jid == NULL || res == NULL)
		goto send;

	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto send;
	g_free(jid);

	resource = xmpp_rosters_find_resource(user, res);
	if (resource == NULL)
		goto send;
	g_free(res);
	
	/* stop composing */
	if (resource->composing_id != NULL) {
		child = lm_message_node_add_child(msg->node, "x", NULL);
		lm_message_node_set_attribute(child, "xmlns", XMLNS_EVENT);
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

send:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

void
own_presence(XMPP_SERVER_REC *server, const int show, const char *status,
    const int priority)
{
	GSList *tmp;
	LmMessage *msg;
	const char *show_str;
	char *status_recoded, *priority_str;

	g_return_if_fail(IS_XMPP_SERVER(server));

	if (!xmpp_presence_changed(show, server->show, status,
	    server->away_reason, priority, server->priority))
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

	default:
		/* unaway */
		if (server->usermode_away)
			signal_emit("event 305", 2, server, server->nick);

		show_str = NULL;
		server->show = XMPP_PRESENCE_AVAILABLE;
		g_free_and_null(server->away_reason);
	}


	/* away */
	if (show_str != NULL) {
		signal_emit("event 306", 2, server, server->nick);

		server->show = show;
		g_free(server->away_reason);
		server->away_reason = g_strdup(status);

		status_recoded = (server->away_reason != NULL) ?
		    xmpp_recode_out(server->away_reason) : NULL;
	}

	if (!xmpp_priority_out_of_bound(priority))
		server->priority = priority;
	priority_str = g_strdup_printf("%d", server->priority);

	/* send presence to the server */
	msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);
	lm_message_node_add_child(msg->node, "show", show_str);
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
}


/*
 * XEP-0022: Message Events
 */

static void
composing_start(XMPP_SERVER_REC *server, const char *full_jid)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *dest_recoded, *jid, *res;
	const char *id;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);

	dest_recoded = xmpp_recode_out(full_jid);
	msg = lm_message_new_with_sub_type(dest_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(dest_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_EVENT);

	lm_message_node_add_child(child, "composing", NULL);

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_rosters_find_resource(user, res);
	if (resource != NULL) {
		id = lm_message_node_get_attribute(msg->node, "id");
		lm_message_node_add_child(child, "id", id);
		g_free_and_null(resource->composing_id);
		resource->composing_id = g_strdup(id);
	}

out:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
	g_free(res);
}

static void
composing_stop(XMPP_SERVER_REC *server, const char *full_jid)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *full_jid_recoded, *jid, *res;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);

	full_jid_recoded = xmpp_recode_out(full_jid);

	msg = lm_message_new_with_sub_type(full_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(full_jid_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_EVENT);

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_rosters_find_resource(user, res);
	if (resource != NULL && resource->composing_id != NULL) {
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

out:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
	g_free(res);
}


/*
 * XEP-0030: Service Discovery
 */

static int
disco_parse_features(const char *var, XMPP_SERVERS_FEATURES features)
{
	g_return_val_if_fail(var != NULL, 0);

	if (!(features & XMPP_SERVERS_FEATURE_PING) &&
	    g_ascii_strcasecmp(var, XMLNS_PING) == 0)
		return XMPP_SERVERS_FEATURE_PING;
	else
		return 0;
}

static void
disco_servers_services(XMPP_SERVER_REC *server, LmMessageNode *query)
{
	LmMessageNode *item;
	const char *var;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(query != NULL);

	server->features = 0;

	item = query->children;
	while(item != NULL) {
		if (g_ascii_strcasecmp(item->name, "feature") != 0)
			goto next;

		var = lm_message_node_get_attribute(item, "var");
		if (var != NULL)
			server->features |= disco_parse_features(var,
			    server->features);

next:
		item = item->next;
	}
}


/*
 * XEP-0092: Software Version
 */

static void
version_send(XMPP_SERVER_REC *server, const char *to_jid,
    const char *id)
{
	LmMessage *msg;
	LmMessageNode *query_node;
	struct utsname u;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(to_jid != NULL);

	msg = lm_message_new_with_sub_type(to_jid, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);

	if (id != NULL)
		lm_message_node_set_attribute(msg->node, "id", id);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", XMLNS_VERSION);

	if (settings_get_bool("xmpp_send_version")) {
		lm_message_node_add_child(query_node, "name",
		    IRSSI_XMPP_PACKAGE);
		lm_message_node_add_child(query_node, "version",
		    IRSSI_XMPP_VERSION);

		if (uname(&u) == 0)
			lm_message_node_add_child(query_node, "os", u.sysname);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

static void
version_handle(XMPP_SERVER_REC *server, const char *jid,
    LmMessageNode *node)
{
	LmMessageNode *child;
	char *value;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(jid != NULL);
	g_return_if_fail(node != NULL);

	signal_emit("xmpp begin of version", 2, server, jid);

	child = node->children;
	while(child != NULL) {

		if (child->value == NULL)
			goto next;

		value = xmpp_recode_in(child->value);
		g_strstrip(value);

		signal_emit("xmpp version value", 4, server, jid, child->name,
		    value);

		g_free(value);

next:
		child = child->next;
	}

	signal_emit("xmpp end of version", 2, server, jid);
}


/*
 * XEP-0054: vcard-temp
 */

static void
vcard_handle(XMPP_SERVER_REC *server, const char *jid,
    LmMessageNode *node)
{
	LmMessageNode *child, *subchild;
	const char *adressing;
	char *value;

	signal_emit("xmpp begin of vcard", 2, server, jid);

	child = node->children;
	while(child != NULL) {

		/* ignore avatar */
		if (g_ascii_strcasecmp(child->name, "PHOTO") == 0)
			goto next;

		if (child->value != NULL) {
			value = xmpp_recode_in(child->value);
			g_strstrip(value);

			signal_emit("xmpp vcard value", 4, server, jid,
			     child->name, value);

			g_free(value);
			goto next;
		}

		/* find the adressing type indicator */
		subchild = child->children;
		adressing = NULL;
		while(subchild != NULL && adressing == NULL) {
			if (subchild->value == NULL && (
			    g_ascii_strcasecmp(subchild->name , "HOME") == 0 ||
			    g_ascii_strcasecmp(subchild->name , "WORK") == 0))
				adressing = subchild->name;

			subchild = subchild->next;
		}

		subchild = child->children;
		while(subchild != NULL) {
			
			if (subchild->value != NULL) {
				value = xmpp_recode_in(subchild->value);
				g_strstrip(value);

				signal_emit("xmpp vcard subvalue", 6, server,
				    jid, child->name, adressing,
				    subchild->name, value);

				g_free(value);
			}

			subchild = subchild->next;
		}

next:
		child = child->next;
	}

	signal_emit("xmpp end of vcard", 2, server, jid);
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
	char *jid, *text;

	server = XMPP_SERVER(user_data);
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

		child = lm_message_node_get_child(msg->node, "subject");
		if (child != NULL) {
			text = xmpp_recode_in(child->value);
			signal_emit("message private", 4, server, text,
			    jid, jid);
			g_free(text);
		}

		child = lm_message_node_get_child(msg->node, "body");
		if (child != NULL) {
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
		break;

	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
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
		
		child = lm_message_node_get_child(msg->node, "body");
		if (child != NULL) {
			XMPP_CHANNEL_REC *channel;
			char *channel_name, *nick;

			channel_name = xmpp_extract_channel(jid);
			nick =  xmpp_extract_resource(jid);

			/* it's my own message, so ignore it */
			channel = xmpp_channel_find(server, channel_name);
			if (channel == NULL || nick == NULL ||
			    (channel != NULL &&
			    strcmp(nick, channel->nick) == 0)) {
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
				signal_emit("message public", 5, server, text,
				   nick, "", channel_name);

			g_free(channel_name);
			g_free(nick);
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
handle_presence(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;
	LmMessageNode *child, *subchild, *show, *priority;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
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

			if (!channel->joined) {
				if (g_ascii_strcasecmp(code, "401") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_PASSWORD_INVALID_OR_MISSING);
				else if (g_ascii_strcasecmp(code, "403") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_USER_BANNED);
				else if (g_ascii_strcasecmp(code, "404") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_ROOM_NOT_FOUND);
				else if (g_ascii_strcasecmp(code, "405") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_ROOM_CREATION_RESTRICTED);
				else if (g_ascii_strcasecmp(code, "406") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_USE_RESERVED_ROOM_NICK);
				else if (g_ascii_strcasecmp(code, "407") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_NOT_ON_MEMBERS_LIST);
				else if (g_ascii_strcasecmp(code, "409") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_NICK_IN_USE);
				else if (g_ascii_strcasecmp(code, "503") == 0)
					signal_emit("xmpp channel joinerror", 2,
					    channel, XMPP_CHANNELS_ERROR_MAXIMUM_USERS_REACHED);
			} else {
				if (g_ascii_strcasecmp(code, "409") == 0)
					signal_emit("xmpp channel nick in use",
					    2, channel, "");
			}

		/* general */
		} else
			signal_emit("xmpp presence error", 2, server, jid);

		break;

	case LM_MESSAGE_SUB_TYPE_SUBSCRIBE:
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
			GSList *x;
			const char *item_affiliation, *item_role;
			char *nick, *item_jid, *item_nick;

			nick = xmpp_extract_nick(jid);
			item_affiliation = item_role = NULL;
			item_jid = item_nick = NULL;

			x = lm_message_node_find_childs(msg->node, "x");
			if (x != NULL &&
			    lm_message_nodes_attribute_found(x, "xmlns",
			    XMLNS_MUC_USER, &child) && child != NULL) {

				if ((subchild = lm_message_node_get_child(
				    child, "item")) != NULL) {
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
				}
			}
			g_slist_free(x);

			signal_emit("xmpp channel nick event", 6, channel,
			    (item_nick != NULL) ? item_nick : nick,
			    item_jid, item_affiliation, item_role);

			show = lm_message_node_get_child(msg->node, "show");
			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;

			signal_emit("xmpp channel nick presence", 4, channel,
			    (item_nick != NULL) ? item_nick : nick,
			    (show != NULL) ? show->value : NULL, text);

			g_free(item_jid);
			g_free(item_nick);
			g_free(text);
			g_free(nick);

		/* general */
		} else {
			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;

			show = lm_message_node_get_child(msg->node, "show");
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
			GSList *x;
			const char *status_code;
			char *nick, *status, *reason, *actor, *item_nick;

			nick = xmpp_extract_nick(jid);
			status_code = NULL;
			status = reason = actor = item_nick = NULL;

			x = lm_message_node_find_childs(msg->node, "x");
			if (x != NULL &&
			    lm_message_nodes_attribute_found(x, "xmlns",
			    XMLNS_MUC_USER, &child) && child != NULL) {

				/* in <X> */
				if ((subchild = lm_message_node_get_child(
				    child, "status")) != NULL) {
					status =
					    xmpp_recode_in(subchild->value);
					status_code =
					    lm_message_node_get_attribute(
					    subchild, "code");
				}

				/* in <x> */
				if ((subchild = lm_message_node_get_child(
				    child, "item")) != NULL)
					item_nick = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    subchild, "nick"));

				/* in <item> */
				if ((child = lm_message_node_get_child(
				    subchild, "reason")) != NULL)
					reason = xmpp_recode_in(child->value);

				/* in <item */
				if ((child = lm_message_node_get_child(
				    subchild, "actor")) != NULL)
					actor = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    child, "jid"));
			}
			g_slist_free(x);
			
			if (status_code != NULL) {

			 	if (g_ascii_strcasecmp(status_code,
				    "303") == 0 && item_nick != NULL)
					signal_emit("xmpp channel nick",
					    5, channel, nick, item_nick);

				else if (g_ascii_strcasecmp(status_code,
				    "307") == 0)
					signal_emit("xmpp channel nick kicked",
					    4, channel, nick, actor, reason);
					
				/* ban */
				else if (g_ascii_strcasecmp(status_code,
				    "301") == 0)
					signal_emit("xmpp channel nick kicked",
					    4, channel, nick, actor, reason);

			} else
				signal_emit("xmpp channel nick part", 3,
				    channel, nick, status);

			g_free(item_nick);
			g_free(status);
			g_free(reason);
			g_free(actor);
			g_free(nick);

		/* general */
		} else {
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
	LmMessageNode *child;
	const char *xmlns;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
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
	    lm_message_node_get_attribute(child, "xmlns") : NULL;

	switch (lm_message_get_sub_type(msg)) {

	case LM_MESSAGE_SUB_TYPE_GET:
		/* XEP-0092: Software Version */
		if (xmlns != NULL &&
		    g_ascii_strcasecmp(xmlns, XMLNS_VERSION) == 0)
			version_send(server, jid,
			    lm_message_node_get_attribute(msg->node, "id"));
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

	g_free(jid);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
handlers_register(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

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

static void
handlers_unregister(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	lm_message_handler_unref(server->hmessage);
	lm_message_handler_unref(server->hpresence);
	lm_message_handler_unref(server->hiq);
}

void
xmpp_protocol_init(void)
{
	signal_add_first("server connected", (SIGNAL_FUNC)handlers_register);
	signal_add("server disconnected",
	    (SIGNAL_FUNC)handlers_unregister);
	signal_add_first("xmpp own_presence", (SIGNAL_FUNC)own_presence);
	signal_add_first("xmpp server disco",
	    (SIGNAL_FUNC)disco_servers_services);
	signal_add("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_add("xmpp composing stop", (SIGNAL_FUNC)composing_stop);

	settings_add_int("xmpp", "xmpp_priority", 0);
	settings_add_bool("xmpp", "xmpp_send_version", TRUE);
	settings_add_bool("xmpp", "xmpp_raw_window", FALSE);
}

void
xmpp_protocol_deinit(void)
{
	signal_remove("server connected",
	    (SIGNAL_FUNC)handlers_register);
	signal_remove("server disconnected",
	    (SIGNAL_FUNC)handlers_unregister);
	signal_remove("xmpp own_presence", (SIGNAL_FUNC)own_presence);
	signal_remove("xmpp server disco", (SIGNAL_FUNC)disco_servers_services);
	signal_remove("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_remove("xmpp composing stop", (SIGNAL_FUNC)composing_stop);
}
