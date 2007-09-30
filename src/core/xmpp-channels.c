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

#include <string.h>

#include "module.h"
#include "commands.h"
#include "nicklist.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-channels.h"
#include "xmpp-tools.h"

/*
 * XEP-0045: Multi-User Chat
 */

CHANNEL_REC *
xmpp_channel_create(XMPP_SERVER_REC *server, const char *name,
    const char *visible_name, int automatic)
{
	XMPP_CHANNEL_REC *rec;

	g_return_val_if_fail(server == NULL || IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	rec = g_new0(XMPP_CHANNEL_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;

	rec->nick = NULL;
	rec->oldnick = NULL;

	channel_init((CHANNEL_REC *)rec, (SERVER_REC *)server, name,
	    visible_name, automatic);

	return (CHANNEL_REC *)rec;
}

void
xmpp_channel_send_message(XMPP_SERVER_REC *server, const char *name,
    const char *message)
{
	LmMessage *msg;
	char *room, *message_recoded;
	GError *error = NULL;

	g_return_if_fail(server != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(message != NULL);

	room = xmpp_recode_out(name);
	msg = lm_message_new_with_sub_type(room,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_GROUPCHAT);
	g_free(room);

	message_recoded = xmpp_recode_out(message);
	lm_message_node_add_child(msg->node, "body", message_recoded);
	g_free(message_recoded);

	lm_connection_send(server->lmconn, msg, &error);
	lm_message_unref(msg);

	if (error != NULL) {
		signal_emit("message xmpp error", 3, server, name,
		    error->message);
		g_free(error);
	}
}

static CHANNEL_REC *
channel_find_server(SERVER_REC *server, const char *channel_name)
{
	GSList *tmp;
	CHANNEL_REC *channel;

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;

		if (channel->chat_type != server->chat_type)
			continue;

		if (g_strcasecmp(channel_name, channel->name) == 0)
			return channel;
	}

	return NULL;
}

static void
send_nick(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room, *room_recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);

	if (!xmpp_server_is_alive(server))
		return;

	room = g_strconcat(channel->name, "/", channel->nick, NULL);
	room_recoded = xmpp_recode_out(room);
	g_free(room);

	msg = lm_message_new(room_recoded, LM_MESSAGE_TYPE_PRESENCE);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns",
	    "http://jabber.org/protocol/muc");

	if (!channel->joined) {
		/* no history */
		child = lm_message_node_add_child(child, "history", NULL);
		lm_message_node_set_attribute(child, "maxchars", "0");
	}

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
}

static void
send_join(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room_recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);

	if (!xmpp_server_is_alive(server))
		return;

	send_nick(server, channel);

	/* request channel properties */
	room_recoded = xmpp_recode_out(channel->name);
	msg = lm_message_new_with_sub_type(room_recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns",
	    "http://jabber.org/protocol/disco#info");
	
	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);

}

static void
send_part(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *reason)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room, *room_recoded, *reason_recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);

	if (!xmpp_server_is_alive(server))
		return;

	room = g_strconcat(channel->name, "/", channel->nick, NULL);
	room_recoded = xmpp_recode_out(room);
	g_free(room);

	msg = lm_message_new_with_sub_type(room_recoded,
	    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_UNAVAILABLE);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns",
	    "http://jabber.org/protocol/muc");

	if (reason != NULL) {
		reason_recoded = xmpp_recode_out(reason);
		child = lm_message_node_add_child(msg->node, "status",
		    reason_recoded);
		g_free(reason_recoded);
	}

	lm_connection_send(server->lmconn, msg, NULL);
	lm_message_unref(msg);
}

static void
channels_join(XMPP_SERVER_REC *server, const char *data, int automatic)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp, *channels;
	void *free_arg;

	g_return_if_fail(server != NULL);
	g_return_if_fail(data != NULL);

	if (!xmpp_server_is_alive(server) || *data == '\0')
		return;

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_GETREST,
	    &channels))
		return;
	
	chanlist = g_strsplit(channels, ",", -1);

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		channel = xmpp_channel_find(server, *tmp);
		if (channel == NULL) {

			channel = (XMPP_CHANNEL_REC *)
			    xmpp_channel_create(server, *tmp, NULL, automatic);

			
			channel->nick = g_strdup(server->nick);

			send_join(server, channel);
		}
	}
	
	g_strfreev(chanlist);
	cmd_params_free(free_arg);
}

static void
channels_part(XMPP_SERVER_REC *server, const char *channels,
    const char *reason)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channels != NULL);

	chanlist = g_strsplit(channels, ",", -1);

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		channel = xmpp_channel_find(server, *tmp);
		if (channel != NULL) {
			send_part(server, channel, reason);

			channel_destroy(CHANNEL(channel));
		}
	}

	g_strfreev(chanlist);
}

static void
channels_nick(XMPP_SERVER_REC *server, const char *channels, const char *nick)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channels != NULL);
	g_return_if_fail(nick != NULL);

	chanlist = g_strsplit(channels, ",", -1);

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		channel = xmpp_channel_find(server, *tmp);
		if (channel != NULL) {
			g_free(channel->oldnick);
			channel->oldnick = channel->nick;
			channel->nick = g_strdup(nick);

			send_nick(server, channel);
		}
	}

	g_strfreev(chanlist);
}

static void
sig_server_connected(SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	server->channel_find_func = channel_find_server;
	server->channels_join = (void (*)(SERVER_REC *, const char *, int))
	    channels_join;
}

static void
sig_channel_created(XMPP_CHANNEL_REC *channel)
{
	if (!IS_XMPP_CHANNEL(channel))
		return;

	if (channel->nicks != NULL)
		g_hash_table_destroy(channel->nicks);
	channel->nicks = g_hash_table_new((GHashFunc)g_str_hash,
	    (GCompareFunc)g_str_equal);
}

static void
sig_channel_destroyed(XMPP_CHANNEL_REC *channel)
{
	if (!IS_XMPP_CHANNEL(channel))
		return;

	/* destroying channel without actually having left it yet */
	if (!channel->server->disconnected && !channel->left) {
		signal_emit("command part", 3, "", channel->server, channel);
	}

	g_free(channel->nick);
	g_free(channel->oldnick);
}

static void
set_topic(XMPP_SERVER_REC *server, const char *channel_name, const char *topic,
    const char *nick_name)
{
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(channel_name != NULL);

	channel = xmpp_channel_find(server, channel_name);
	if (channel == NULL)
		return;

	g_free(channel->topic);
	channel->topic = (*topic != '\0') ? g_strdup(topic) : NULL;

	g_free(channel->topic_by);
	channel->topic_by = g_strdup(nick_name);

	signal_emit("channel topic changed", 1, channel);

	if (channel->joined)
		signal_emit("message topic", 5, server, channel->name,
		    channel->topic, channel->topic_by, "");
	else {
		char *data = g_strconcat(" ", channel->name, " :",
		    channel->topic, NULL);
		signal_emit("event 332", 2, server, data);
		g_free(data);
	}
}

static void
set_modes(NICK_REC *nick, const char *affiliation, const char *role)
{
	g_return_if_fail(nick != NULL);

	g_debug(nick->nick);

	if (affiliation != NULL) {
		if (g_ascii_strcasecmp(affiliation, "owner") == 0) {
			nick->other = '&';
			nick->op = TRUE;
		} else if (g_ascii_strcasecmp(affiliation, "admin") == 0) {
			nick->other = NULL;
			nick->op = TRUE;
		} else {
			nick->other = NULL;
			nick->op = FALSE;
		}
	}

	if (role != NULL) {
		if (g_ascii_strcasecmp(role, "participant") == 0) {
			nick->halfop = FALSE; 
			nick->voice = TRUE; 
		} else if (g_ascii_strcasecmp(role, "moderator") == 0) {
			g_debug("halfop");
			nick->halfop = TRUE;
			nick->voice = TRUE;
		} else {
			nick->halfop = FALSE;
			nick->voice = FALSE;
		}
	}
}

static NICK_REC *
insert_nick(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *affiliation, const char *role)
{
	NICK_REC *nick;

	g_return_val_if_fail(channel != NULL, NULL);
	g_return_val_if_fail(nick_name != NULL, NULL);

	nick = g_new0(NICK_REC, 1);
	nick->nick = g_strdup(nick_name);
	set_modes(nick, affiliation, role);

	nicklist_insert(CHANNEL(channel), nick);

	if (strcmp(nick->nick, channel->nick) == 0) {
		nicklist_set_own(CHANNEL(channel), nick);
		channel->chanop = channel->ownnick->op;
	}

	return nick;
}

static void
nick_event(XMPP_SERVER_REC *server, const char *channel_name,
    const char *nick_name, const char *affiliation, const char *role)
{
	XMPP_CHANNEL_REC *channel;
	NICK_REC *nick;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel_name != NULL);
	g_return_if_fail(nick_name != NULL);

	g_debug("event");

	channel = xmpp_channel_find(server, channel_name);
	if (channel == NULL)
		 return;

	nick = nicklist_find(CHANNEL(channel), nick_name);
	if (nick == NULL)
		signal_emit("xmpp channel nick add", 5, server, channel,
		    nick_name, affiliation, role);
	else
		signal_emit("xmpp channel nick mode", 5, server, channel,
		    nick, affiliation, role);
}

static void
nick_add(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *nick_name, const char *affiliation, const char *role)
{
	NICK_REC *nick;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick_name != NULL);

	nick = nicklist_find(CHANNEL(channel), nick_name);
	if (nick != NULL)
		return;

	nick = insert_nick(channel, nick_name, affiliation, role);

	if (channel->names_got ||
	    strcmp(nick_name, channel->nick) == 0) {
		char *addr = g_strconcat(channel->name, "/", nick->nick, NULL);
		signal_emit("message join", 4, server, channel->name,
		    nick->nick, addr);
		g_free(addr);
	}
}

static void
nick_remove(XMPP_SERVER_REC *server, const char *channel_name,
    const char *nick_name, const char *status)
{
	XMPP_CHANNEL_REC *channel;
	NICK_REC *nick;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel_name != NULL);
	g_return_if_fail(nick_name != NULL);

	channel = xmpp_channel_find(server, channel_name);
	if (channel == NULL)
		return;

	nick = nicklist_find(CHANNEL(channel), nick_name);
	if (nick == NULL)
		return;

	nicklist_remove(CHANNEL(channel), nick);

	if (channel->oldnick == NULL ||
	    strcmp(nick_name, channel->oldnick) != 0) {
		char *addr = g_strconcat(channel->name, "/", nick_name, NULL);
		signal_emit("message part", 5, server, channel->name,
		    nick_name, addr, status);
		g_free(addr);
	}
}

static void
nick_mode(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    NICK_REC *nick, const char *affiliation, const char *role)
{
	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);

	set_modes(nick, affiliation, role);

	/* event */
}

static void
nick_change(XMPP_SERVER_REC *server, const char *channel_name,
    const char *oldnick, const char *newnick, const char *affiliation,
    const char *role)
{
	XMPP_CHANNEL_REC *channel;
	NICK_REC *nick;
	
	g_return_if_fail(server != NULL);
	g_return_if_fail(channel_name != NULL);
	g_return_if_fail(oldnick != NULL);
	g_return_if_fail(newnick != NULL);

	channel = xmpp_channel_find(server, channel_name);
	if (channel == NULL)
		return;

	g_debug("!!!change!!!, %s", oldnick);

	nick = nicklist_find(CHANNEL(channel), oldnick);
	if (nick == NULL)
		return;

	nicklist_remove(CHANNEL(channel), nick);
	insert_nick(channel, newnick, affiliation, role);

	signal_emit("message xmpp channel nick", 4, server,
	    channel, newnick, oldnick);
}

static void
joined(XMPP_SERVER_REC *server, const char *channel_name)
{
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel_name != NULL);

	channel = xmpp_channel_find(server, channel_name);
	if (channel == NULL || channel->joined)
		return;

	if (channel->ownnick == NULL)
		return;

	channel->names_got = TRUE;
	channel->joined = TRUE;

	signal_emit("channel joined", 1, channel);
	signal_emit("channel sync", 1, channel);
}

void
xmpp_channels_init(void)
{
	signal_add("xmpp channels part", (SIGNAL_FUNC)channels_part);
	signal_add("xmpp channels nick", (SIGNAL_FUNC)channels_nick);
	signal_add_first("server connected",
	    (SIGNAL_FUNC)sig_server_connected);
	signal_add_last("channel created", (SIGNAL_FUNC)sig_channel_created);
	signal_add("channel destroyed", (SIGNAL_FUNC)sig_channel_destroyed);
	signal_add("xmpp channel topic", (SIGNAL_FUNC)set_topic);
	signal_add("xmpp channel nick event", (SIGNAL_FUNC)nick_event);
	signal_add("xmpp channel nick add", (SIGNAL_FUNC)nick_add);
	signal_add("xmpp channel nick remove", (SIGNAL_FUNC)nick_remove);
	signal_add("xmpp channel nick mode", (SIGNAL_FUNC)nick_mode);
	signal_add("xmpp channel nick change", (SIGNAL_FUNC)nick_change);
	signal_add("xmpp channel joined", (SIGNAL_FUNC)joined);

	settings_add_str("xmpp", "muc_default_nick", "");
}

void
xmpp_channels_deinit(void)
{
	signal_remove("xmpp channels part", (SIGNAL_FUNC)channels_part);
	signal_remove("xmpp channels nick", (SIGNAL_FUNC)channels_nick);
	signal_remove("server connected", (SIGNAL_FUNC)sig_server_connected);
	signal_remove("channel created", (SIGNAL_FUNC)sig_channel_created);
	signal_remove("channel destroyed",(SIGNAL_FUNC)sig_channel_destroyed);
	signal_remove("xmpp channel topic", (SIGNAL_FUNC)set_topic);
	signal_remove("xmpp channel nick event", (SIGNAL_FUNC)nick_event);
	signal_remove("xmpp channel nick add", (SIGNAL_FUNC)nick_add);
	signal_remove("xmpp channel nick remove", (SIGNAL_FUNC)nick_remove);
	signal_remove("xmpp channel nick mode", (SIGNAL_FUNC)nick_mode);
	signal_remove("xmpp channel nick change", (SIGNAL_FUNC)nick_change);
	signal_remove("xmpp channel joined", (SIGNAL_FUNC)joined);
}
