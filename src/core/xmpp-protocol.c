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

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"
#include "loudmouth-tools.h"

void
xmpp_send_message(XMPP_SERVER_REC *server, const char *to_jid,
    const char *message)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *jid, *res, *to_full_jid, *to_jid_recoded, *message_recoded;
	GError *error = NULL;

	g_return_if_fail(server != NULL);
	g_return_if_fail(to_jid != NULL);
	g_return_if_fail(message != NULL);

	to_full_jid = xmpp_get_full_jid(server, to_jid);
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

	user = xmpp_find_user(server, jid, NULL);
	if (user == NULL)
		goto send;
	g_free(jid);

	resource = xmpp_find_resource(user, res);
	if (resource == NULL)
		goto send;
	g_free(res);
	
	/* stop composing */
	if (resource->composing_id != NULL) {
		child = lm_message_node_add_child(msg->node, "x", NULL);
		lm_message_node_set_attribute(child, "xmlns",
		    "jabber:x:event");
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

send:
	lm_connection_send(server->lmconn, msg, &error);
	lm_message_unref(msg);

	if (error != NULL) {
		signal_emit("message xmpp error", 3, server, to_jid,
		    error->message);
		g_free(error);
	}
}

void
own_presence(XMPP_SERVER_REC *server, const int show, const char *status,
    const int priority)
{
	LmMessage *msg;
	char *status_recoded, *priority_str;

	g_return_if_fail(server != NULL);

	if (!xmpp_presence_changed(show, server->show, status,
	    server->away_reason, priority, server->priority))
		return;

	msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);

	switch (show) {
	case XMPP_PRESENCE_AWAY:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_AWAY]);
		break;

	case XMPP_PRESENCE_CHAT:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_CHAT]); 
		break;

	case XMPP_PRESENCE_DND:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_DND]);
		break;

	case XMPP_PRESENCE_XA:
		lm_message_node_add_child(msg->node, "show",
		    xmpp_presence_show[XMPP_PRESENCE_XA]);
		break;

	default:
		/* unaway */
		if (server->usermode_away)
			signal_emit("event 305", 2, server, server->nick);

		server->show = XMPP_PRESENCE_AVAILABLE;
		g_free_and_null(server->away_reason);
	}

	/* away */
	if (lm_message_node_get_child(msg->node, "show") != NULL) {
		signal_emit("event 306", 2, server, server->nick);

		server->show = show;
		g_free(server->away_reason);
		server->away_reason = g_strdup(status);

		if (server->away_reason != NULL) {
			status_recoded = xmpp_recode_out(server->away_reason);
			lm_message_node_add_child(msg->node, "status",
			    status_recoded);
			g_free(status_recoded);
		}
	}

	if (!xmpp_priority_out_of_bound(priority))
		server->priority = priority;
	priority_str = g_strdup_printf("%d", server->priority);
	lm_message_node_add_child(msg->node, "priority", priority_str);
	g_free(priority_str);

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
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
	char *full_jid_recoded, *jid, *res;
	const char *id;

	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);

	full_jid_recoded = xmpp_recode_out(full_jid);

	msg = lm_message_new_with_sub_type(full_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", "jabber:x:event");
	g_free(full_jid_recoded);

	lm_message_node_add_child(child, "composing", NULL);

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_find_user(server, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_find_resource(user, res);
	if (resource != NULL) {
		id = lm_message_node_get_attribute(msg->node, "id");
		lm_message_node_add_child(child, "id", id);
		g_free_and_null(resource->composing_id);
		resource->composing_id = g_strdup(id);
	}

out:
	lm_connection_send(server->lmconn, msg, NULL);
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

	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);

	full_jid_recoded = xmpp_recode_out(full_jid);

	msg = lm_message_new_with_sub_type(full_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(full_jid_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", "jabber:x:event");

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_find_user(server, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_find_resource(user, res);
	if (resource != NULL && resource->composing_id != NULL) {
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

out:
	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
	g_free(res);
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

	g_return_if_fail(server != NULL);
	g_return_if_fail(to_jid != NULL);

	msg = lm_message_new_with_sub_type(to_jid, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);

	if (id != NULL)
		lm_message_node_set_attribute(msg->node, "id", id);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:version");

	if (settings_get_bool("xmpp_send_version")) {
		lm_message_node_add_child(query_node, "name",
		    IRSSI_XMPP_PACKAGE);
		lm_message_node_add_child(query_node, "version",
		    IRSSI_XMPP_VERSION);

		if (uname(&u) == 0)
			lm_message_node_add_child(query_node, "os", u.sysname);
	}

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
}

static void
version_handle(XMPP_SERVER_REC *server, const char *jid,
    LmMessageNode *node)
{
	LmMessageNode *child;
	char *value;

	g_return_if_fail(server != NULL);
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
	LmMessageNode *child, *subchild;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	switch (lm_message_get_sub_type(msg)) {
	
	case LM_MESSAGE_SUB_TYPE_ERROR:
		child = lm_message_node_get_child(msg->node, "body");
		if (child == NULL)
			signal_emit("message xmpp error", 2, server, jid);
		else {
			text = xmpp_recode_in(child->value);
			signal_emit("message xmpp error", 3, server, jid,
			    text);
			g_free(text);
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
			text = xmpp_recode_in(child->value);

			signal_emit("xmpp channel topic", 4, server,
			    channel_name, text, nick);

			g_free(channel_name);
			g_free(nick);
			g_free(text);

			goto out;
		}
		
		child = lm_message_node_get_child(msg->node, "body");
		if (child != NULL) {
			XMPP_CHANNEL_REC *channel;
			char *channel_name, *nick;

			channel_name = xmpp_extract_channel(jid);
			nick =  xmpp_extract_resource(jid);

			/* it's my own message, so ignore it */
			channel = xmpp_channel_find(server, channel_name);
			if (nick == NULL || (channel != NULL &&
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
	LmMessageNode *child, *subchild, *show, *priority;
	char *jid, *text;

	server = XMPP_SERVER(user_data);
	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	switch (lm_message_get_sub_type(msg)) {

	/* an error occured when the server try to get the pressence */
	case LM_MESSAGE_SUB_TYPE_ERROR:
		signal_emit("xmpp presence error", 2, server, jid);
		break;

	/* the user wants to add you in his/her roster */
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

	/* the user change his presence */
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		/* from channel */
		text = xmpp_extract_channel(jid);
		if (xmpp_channel_find(server, text) != NULL) {
			GSList *x;
			const char *item_affiliation, *item_role;
			char *nick, *item_jid, *item_nick;

			nick = xmpp_extract_nick(jid);
			item_affiliation = item_role = NULL;
			item_jid = item_nick = NULL;

			x = lm_message_node_find_childs(msg->node, "x");
			if (x != NULL &&
			    lm_message_nodes_attribute_found(x, "xmlns",
			    "http://jabber.org/protocol/muc#user", &child)) {

				subchild = lm_message_node_get_child(child,
				    "item");
				if (subchild != NULL) {
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

			signal_emit("xmpp channel nick event", 6, server, text,
			    (item_nick != NULL) ? item_nick : nick, item_jid,
			    item_affiliation, item_role);

			g_free(item_jid);
			g_free(item_nick);
			g_free(nick);

		/* from roster */
		} else {
			g_free(text);

			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;

			show = lm_message_node_get_child(msg->node, "show");
			priority = lm_message_node_get_child(msg->node,
			    "priority");

			signal_emit("xmpp presence update", 5, server, jid,
			    (show != NULL) ? show->value : NULL, text,
			    (priority != NULL) ? priority->value : NULL);
		}

		g_free(text);
		break;

	/* the user is disconnecting */
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		/* from channel */
		text = xmpp_extract_channel(jid);
		if (xmpp_channel_find(server, text) != NULL) {
			GSList *x;
			const char *item_affiliation, *item_role, *status_code;
			char *nick, *status, *reason, *item_nick;

			nick = xmpp_extract_nick(jid);
			item_affiliation = item_role = status_code = NULL;
			status = reason = item_nick = NULL;

			/* changing name */
			x = lm_message_node_find_childs(msg->node, "x");
			if (x != NULL &&
			    lm_message_nodes_attribute_found(x, "xmlns",
			    "http://jabber.org/protocol/muc#user", &child)) {

				subchild = lm_message_node_get_child(child,
				    "item");
				if (subchild != NULL) {
					item_affiliation =
					    lm_message_node_get_attribute(
					    subchild, "affiliation");
					item_role =
					    lm_message_node_get_attribute(
					    subchild, "role");
					item_nick = xmpp_recode_in(
					    lm_message_node_get_attribute(
					    subchild, "nick"));
				}

				subchild = lm_message_node_get_child(subchild,
				    "reason");
				if (subchild != NULL)
					reason =
					    xmpp_recode_in(subchild->value);

				subchild = lm_message_node_get_child(child,
				    "status");
				if (subchild != NULL) {
					status =
					    xmpp_recode_in(subchild->value);
					status_code =
					    lm_message_node_get_attribute(
					    subchild, "code");
				}
			}
			g_slist_free(x);

			/* status code 303, change nick */
			if (status_code != NULL) {
			 	if (g_ascii_strcasecmp(status_code,
				    "303") == 0 && item_nick != NULL)
					signal_emit("xmpp channel nick change",
					    6, server, text, nick, item_nick,
					    item_affiliation, item_role);

				/* kick */
				else if (g_ascii_strcasecmp(status_code,
				    "307") == 0)
					signal_emit("xmpp channel nick kick",
					    4, server, text, nick, reason);
					
				/* ban */
				else if (g_ascii_strcasecmp(status_code,
				    "301") == 0)
					signal_emit("xmpp channel nick kick", 4,
					    server, text, nick, reason);

			} else
				signal_emit("xmpp channel nick remove", 4,
				    server, text, nick, status);

			g_free(item_nick);
			g_free(status);
			g_free(reason);
			g_free(nick);

		/* from roster */
		} else {
			g_free(text);

			child = lm_message_node_get_child(msg->node, "status");
			text = (child != NULL) ?
			    xmpp_recode_in(child->value) : NULL;
			signal_emit("xmpp presence unavailable", 3, server,
			    jid, text);
		}

		g_free(text);
		break;

	default:
		break;
	}

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
	char *jid;

	server = XMPP_SERVER(user_data);
	jid = xmpp_recode_in(lm_message_node_get_attribute(msg->node, "from"));

	child = lm_message_node_get_child(msg->node, "query");
	if (child == NULL)
		child = lm_message_node_get_child(msg->node, "vCard");

	xmlns = (child != NULL) ?
	    lm_message_node_get_attribute(child, "xmlns") : NULL;
	if (xmlns == NULL)
		goto out;

	switch (lm_message_get_sub_type(msg)) {

	case LM_MESSAGE_SUB_TYPE_GET:
		/* XEP-0092: Software Version */
		if (g_ascii_strcasecmp(xmlns, "jabber:iq:version") == 0)
			version_send(server, jid,
			    lm_message_node_get_attribute(msg->node, "id"));
		break;

	case LM_MESSAGE_SUB_TYPE_RESULT:
		if (g_ascii_strcasecmp(xmlns, "jabber:iq:roster") == 0)
			signal_emit("xmpp roster update", 2, server, child);

		else if (g_ascii_strcasecmp(xmlns, "jabber:iq:version") == 0)
			version_handle(server, jid, child);

		else if (g_ascii_strcasecmp(xmlns, "vcard-temp") == 0)
			vcard_handle(server, jid, child);

		else if (g_ascii_strcasecmp(xmlns,
		    "http://jabber.org/protocol/disco#info") == 0) {
			signal_emit("xmpp channel joined", 2, server, jid);
		}
		
		break;

	case LM_MESSAGE_SUB_TYPE_SET :
		if (g_ascii_strcasecmp(xmlns, "jabber:iq:roster") == 0)
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
handlers_register(XMPP_SERVER_REC *server)
{
	LmMessageHandler *hmessage, *hpresence, *hiq;

	/* handle message */
	hmessage = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_message, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn, hmessage,
	    LM_MESSAGE_TYPE_MESSAGE, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hmessage);

	/* handle presence */
	hpresence = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_presence, (gpointer)server,
	    NULL);
	lm_connection_register_message_handler(server->lmconn, hpresence,
	    LM_MESSAGE_TYPE_PRESENCE, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hpresence);

	/* handle iq */
	hiq = lm_message_handler_new(
	    (LmHandleMessageFunction)handle_iq, (gpointer)server, NULL);
	lm_connection_register_message_handler(server->lmconn, hiq,
	    LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_NORMAL);
	lm_message_handler_unref(hiq);
}

static void
handlers_unregister(XMPP_SERVER_REC *server)
{
}

void
xmpp_protocol_init(void)
{
	signal_add("xmpp handlers register", (SIGNAL_FUNC)handlers_register);
	signal_add("xmpp handlers unregister",
	    (SIGNAL_FUNC)handlers_unregister);
	signal_add_first("xmpp own_presence", (SIGNAL_FUNC)own_presence);
	signal_add("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_add("xmpp composing stop", (SIGNAL_FUNC)composing_stop);

	settings_add_int("xmpp", "xmpp_priority", 0);
	settings_add_bool("xmpp", "xmpp_send_version", TRUE);
}

void
xmpp_protocol_deinit(void)
{
	signal_remove("xmpp handlers register",
	    (SIGNAL_FUNC)handlers_register);
	signal_remove("xmpp handlers unregister",
	    (SIGNAL_FUNC)handlers_unregister);
	signal_remove("xmpp own_presence", (SIGNAL_FUNC)own_presence);
	signal_remove("xmpp composing start", (SIGNAL_FUNC)composing_start);
	signal_remove("xmpp composing stop", (SIGNAL_FUNC)composing_stop);
}
