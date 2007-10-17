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
#include "settings.h"
#include "signals.h"

#include "xmpp-channels.h"
#include "xmpp-nicklist.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

/*
 * XEP-0045: Multi-User Chat
 */

CHANNEL_REC *
xmpp_channel_create(XMPP_SERVER_REC *server, const char *name,
    const char *visible_name, int automatic, const char *nick)
{
	XMPP_CHANNEL_REC *rec;

	g_return_val_if_fail(server == NULL || IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	rec = g_new0(XMPP_CHANNEL_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;

	rec->nick = g_strdup((nick != NULL) ? nick :
	  (*settings_get_str("nick") != '\0') ?
	  settings_get_str("nick") : server->user);
	rec->features = 0;
	rec->error = FALSE;

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

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
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
send_nick(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *nick)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room, *room_recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);

	if (!xmpp_server_is_alive(server))
		return;

	room = g_strconcat(channel->name, "/", nick, NULL);
	room_recoded = xmpp_recode_out(room);
	g_free(room);

	msg = lm_message_new(room_recoded, LM_MESSAGE_TYPE_PRESENCE);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_MUC);

	/* no history */
	if (!channel->joined) {
		child = lm_message_node_add_child(child, "history", NULL);
		lm_message_node_set_attribute(child, "maxchars", "0");
	}

	lm_send(server, msg, NULL);
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

	send_nick(server, channel, channel->nick);

	/* request channel properties */
	room_recoded = xmpp_recode_out(channel->name);
	msg = lm_message_new_with_sub_type(room_recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_DISCO_INFO);
	
	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

static void
xmpp_channels_join(XMPP_SERVER_REC *server, const char *data, int automatic)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp, *channels, *keys, *channel_name, *nick;
	void *free_arg;

	g_return_if_fail(server != NULL);
	g_return_if_fail(data != NULL);

	if (!xmpp_server_is_alive(server) || *data == '\0')
		return;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST,
	    &channels, &keys))
		return;
	
	chanlist = g_strsplit(channels, ",", -1);
	channel_name = nick = NULL;

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		g_strstrip(*tmp);

		nick = xmpp_extract_nick(*tmp);
		if (nick != NULL)
			channel_name = xmpp_extract_channel(*tmp);

		channel = xmpp_channel_find(server, (channel_name != NULL) ?
		    channel_name : *tmp);
		if (channel == NULL) {
			channel = (XMPP_CHANNEL_REC *)
			    xmpp_channel_create(server,
			    (channel_name != NULL) ? channel_name : *tmp,
			    NULL, automatic, nick);

			send_join(server, channel);
		}

		g_free_and_null(nick);
		g_free_and_null(channel_name);
	}
	
	g_strfreev(chanlist);
	cmd_params_free(free_arg);
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
	lm_message_node_set_attribute(child, "xmlns", XMLNS_MUC);

	if (reason != NULL) {
		reason_recoded = xmpp_recode_out(reason);
		child = lm_message_node_add_child(msg->node, "status",
		    reason_recoded);
		g_free(reason_recoded);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

static void
sig_part(XMPP_SERVER_REC *server, const char *channels,
    const char *reason)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channels != NULL);

	chanlist = g_strsplit(channels, ",", -1);

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		g_strstrip(*tmp);
		channel = xmpp_channel_find(server, *tmp);
		if (channel != NULL) {
			if (!channel->error)
				send_part(server, channel, reason);
			channel->left = TRUE;

			if (channel->ownnick != NULL)
				signal_emit("message part", 5, server,
				    channel->name, channel->ownnick->nick,
				    channel->ownnick->host, reason);

			channel_destroy(CHANNEL(channel));
		}
	}

	g_strfreev(chanlist);
}

static void
sig_own_nick(XMPP_SERVER_REC *server, const char *channels, const char *nick)
{
	XMPP_CHANNEL_REC *channel;
	char **chanlist, **tmp;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channels != NULL);
	g_return_if_fail(nick != NULL);

	chanlist = g_strsplit(channels, ",", -1);

	for (tmp = chanlist; *tmp != NULL; tmp++) {
		g_strstrip(*tmp);
		channel = xmpp_channel_find(server, *tmp);
		if (channel != NULL)
			send_nick(server, channel, nick);
	}

	g_strfreev(chanlist);
}

static void
sig_topic(XMPP_CHANNEL_REC *channel, const char *topic, const char *nick_name)
{
	g_return_if_fail(channel != NULL);

	g_free(channel->topic);
	channel->topic = (topic != NULL && *topic != '\0') ?
	    g_strdup(topic) : NULL;

	g_free(channel->topic_by);
	channel->topic_by = g_strdup(nick_name);

	signal_emit("channel topic changed", 1, channel);

	if (channel->joined)
		signal_emit("message topic", 5, channel->server, channel->name,
		    (channel->topic != NULL) ? channel->topic : "",
		    channel->topic_by, "");
	else {
		char *data = g_strconcat(" ", channel->name, " :",
		    (channel->topic != NULL) ? channel->topic : "", NULL);
		signal_emit("event 332", 2, channel->server, data);
		g_free(data);
	}
}
static void
sig_nick_event(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *full_jid, const char *affiliation, const char *role)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick == NULL)
		signal_emit("xmpp channel nick join", 6, channel, nick_name,
		    full_jid, affiliation, role);
	else 
		signal_emit("xmpp channel nick mode", 6, channel, nick,
		    affiliation, role);
}

static void
sig_nick_join(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *full_jid, const char *affiliation, const char *role)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick != NULL)
		return;

	nick = xmpp_nicklist_insert(channel, nick_name, full_jid);
	xmpp_nicklist_set_modes(nick,
	    xmpp_nicklist_get_affiliation(affiliation),
	    xmpp_nicklist_get_role(role));

	if (channel->names_got || channel->ownnick == NICK(nick)) {
		signal_emit("message join", 4, channel->server, channel->name,
		    nick->nick, nick->host);
		signal_emit("message xmpp channel mode", 5, channel->server,
		    channel, nick->nick, nick->affiliation, nick->role);
	}
}

static void
sig_nick_part(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *status)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick == NULL)
		return;

	signal_emit("message part", 5, channel->server, channel->name,
	    nick->nick, nick->host, status);

	nicklist_remove(CHANNEL(channel), NICK(nick));
}

static void
sig_nick_mode(XMPP_CHANNEL_REC *channel, XMPP_NICK_REC *nick,
    const char *affiliation_str, const char *role_str)
{
	int affiliation, role;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(IS_XMPP_NICK(nick));

	affiliation = xmpp_nicklist_get_affiliation(affiliation_str);
	role = xmpp_nicklist_get_role(role_str);

	if (xmpp_nicklist_modes_changed(nick, affiliation, role)) {
		xmpp_nicklist_set_modes(nick, affiliation, role);

		signal_emit("message xmpp channel mode", 5, channel->server,
		    channel, nick->nick, affiliation, role);
	}
}

static void
sig_nick_presence(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *show_str, const char *status)
{
	XMPP_NICK_REC *nick;
	int show;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick == NULL)
		return;

	show = xmpp_presence_get_show(show_str);
	if (xmpp_presence_changed(show, nick->show, status, nick->status,
	    0, 0)) {
		xmpp_nicklist_set_presence(nick, show, status);

		if (channel->joined && channel->ownnick != NICK(nick)) {
			/* show event */
		}
	}
}

static void
sig_nick_changed(XMPP_CHANNEL_REC *channel, const char *oldnick,
    const char *newnick)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(oldnick != NULL);
	g_return_if_fail(newnick != NULL);

	nick = xmpp_nicklist_find(channel, oldnick);
	if (nick == NULL)
		return;

	xmpp_nicklist_rename(channel, nick, oldnick, newnick);

	if (channel->ownnick == NICK(nick))
		signal_emit("message xmpp channel own_nick", 4,
		    channel->server, channel, nick, oldnick);
	else
		signal_emit("message xmpp channel nick", 4,
		    channel->server, channel, nick, oldnick);

}

static void
sig_nick_kicked(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *actor, const char *reason)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick == NULL)
		return;

	signal_emit("message kick", 6, channel->server, channel->name,
	    nick->nick, (actor != NULL) ? actor : channel->name, nick->host,
	    reason);

	if (channel->ownnick == NICK(nick)) {
		channel->kicked = TRUE;
		channel_destroy(CHANNEL(channel));
	} else
		nicklist_remove(CHANNEL(channel), NICK(nick));
}

static XMPP_CHANNELS_FEATURES
disco_parse_channels_features(const char *var, XMPP_CHANNELS_FEATURES features)
{
	g_return_val_if_fail(var != NULL, 0);

	if (!(features & XMPP_CHANNELS_FEATURE_HIDDEN) &&
	    g_ascii_strcasecmp(var, "muc_hidden") == 0)
		return XMPP_CHANNELS_FEATURE_HIDDEN;

	if (!(features & XMPP_CHANNELS_FEATURE_MEMBERS_ONLY) &&
	    g_ascii_strcasecmp(var, "muc_membersonly") == 0)
		return XMPP_CHANNELS_FEATURE_MEMBERS_ONLY;

	if (!(features & XMPP_CHANNELS_FEATURE_MODERATED) &&
	    g_ascii_strcasecmp(var, "muc_moderated") == 0)
		return XMPP_CHANNELS_FEATURE_MODERATED;

	if (!(features & XMPP_CHANNELS_FEATURE_NONANONYMOUS) &&
	    g_ascii_strcasecmp(var, "muc_nonanonymous") == 0)
		return XMPP_CHANNELS_FEATURE_NONANONYMOUS;

	if (!(features & XMPP_CHANNELS_FEATURE_OPEN) &&
	    g_ascii_strcasecmp(var, "muc_open") == 0)
		return XMPP_CHANNELS_FEATURE_OPEN;

	if (!(features & XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED) &&
	    g_ascii_strcasecmp(var, "muc_passwordprotected") == 0)
		return XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED;

	if (!(features & XMPP_CHANNELS_FEATURE_PERSISTENT) &&
	    g_ascii_strcasecmp(var, "muc_persistent") == 0)
		return XMPP_CHANNELS_FEATURE_PERSISTENT;

	if (!(features & XMPP_CHANNELS_FEATURE_PUBLIC) &&
	    g_ascii_strcasecmp(var, "muc_public") == 0)
		return XMPP_CHANNELS_FEATURE_PUBLIC;

	if (!(features & XMPP_CHANNELS_FEATURE_SEMIANONYMOUS) &&
	    g_ascii_strcasecmp(var, "muc_semianonymous") == 0)
		return XMPP_CHANNELS_FEATURE_SEMIANONYMOUS;

	if (!(features & XMPP_CHANNELS_FEATURE_TEMPORARY) &&
	    g_ascii_strcasecmp(var, "muc_temporary") == 0)
		return XMPP_CHANNELS_FEATURE_TEMPORARY;

	if (!(features & XMPP_CHANNELS_FEATURE_PERSISTENT) &&
	    g_ascii_strcasecmp(var, "muc_unmoderated") == 0)
		return XMPP_CHANNELS_FEATURE_PERSISTENT;

	if (!(features & XMPP_CHANNELS_FEATURE_UNSECURED) &&
	    g_ascii_strcasecmp(var, "muc_unsecured") == 0)
		return XMPP_CHANNELS_FEATURE_UNSECURED;

	else
		return 0;
}

static void
sig_disco(XMPP_CHANNEL_REC *channel, LmMessageNode *query)
{
	LmMessageNode *item;
	const char *var;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(query != NULL);

	channel->features = 0;

	item = query->children;
	while(item != NULL) {

		if (g_ascii_strcasecmp(item->name, "feature") != 0)
			goto next;

		var = lm_message_node_get_attribute(item, "var");
		if (var != NULL)
			channel->features |= disco_parse_channels_features(var,
			    channel->features);

next:
		item = item->next;
	}
	
	/* assume joined when receiving the info for the first time */
	if (!channel->joined && channel->ownnick != NULL) {
		channel->names_got = TRUE;
		channel->joined = TRUE;
		signal_emit("channel joined", 1, channel);
		signal_emit("channel sync", 1, channel);
	}
}

static void
sig_joinerror(XMPP_CHANNEL_REC *channel, int error)
{
	g_return_if_fail(IS_XMPP_CHANNEL(channel));

	channel->error = TRUE;
	signal_emit("xmpp channels part", 3, channel->server, channel->name,
	    NULL);
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

	if (!channel->server->disconnected && !channel->left)
		send_part(channel->server, channel,
		    settings_get_str("part_message"));

	g_free(channel->nick);
}

static void
sig_server_connected(SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;

	server->channel_find_func = channel_find_server;
	server->channels_join = (void (*)(SERVER_REC *, const char *, int))
	    xmpp_channels_join;
}

void
xmpp_channels_init(void)
{
	signal_add("xmpp channels part", (SIGNAL_FUNC)sig_part);
	signal_add("xmpp channels own_nick", (SIGNAL_FUNC)sig_own_nick);
	signal_add("xmpp channel topic", (SIGNAL_FUNC)sig_topic);
	signal_add("xmpp channel nick event", (SIGNAL_FUNC)sig_nick_event);
	signal_add("xmpp channel nick join", (SIGNAL_FUNC)sig_nick_join);
	signal_add("xmpp channel nick part", (SIGNAL_FUNC)sig_nick_part);
	signal_add("xmpp channel nick mode", (SIGNAL_FUNC)sig_nick_mode);
	signal_add("xmpp channel nick presence",
	    (SIGNAL_FUNC)sig_nick_presence);
	signal_add("xmpp channel nick", (SIGNAL_FUNC)sig_nick_changed);
	signal_add("xmpp channel nick kicked", (SIGNAL_FUNC)sig_nick_kicked);
	signal_add("xmpp channel disco", (SIGNAL_FUNC)sig_disco);
	signal_add("xmpp channel joinerror", (SIGNAL_FUNC)sig_joinerror);
	signal_add_last("channel created", (SIGNAL_FUNC)sig_channel_created);
	signal_add("channel destroyed", (SIGNAL_FUNC)sig_channel_destroyed);
	signal_add_first("server connected",
	    (SIGNAL_FUNC)sig_server_connected);
}

void
xmpp_channels_deinit(void)
{
	signal_remove("xmpp channels part", (SIGNAL_FUNC)sig_part);
	signal_remove("xmpp channels own_nick", (SIGNAL_FUNC)sig_own_nick);
	signal_remove("xmpp channel topic", (SIGNAL_FUNC)sig_topic);
	signal_remove("xmpp channel nick event", (SIGNAL_FUNC)sig_nick_event);
	signal_remove("xmpp channel nick join", (SIGNAL_FUNC)sig_nick_join);
	signal_remove("xmpp channel nick part", (SIGNAL_FUNC)sig_nick_part);
	signal_remove("xmpp channel nick mode", (SIGNAL_FUNC)sig_nick_mode);
	signal_remove("xmpp channel nick presence",
	    (SIGNAL_FUNC)sig_nick_presence);
	signal_remove("xmpp channel nick", (SIGNAL_FUNC)sig_nick_changed);
	signal_remove("xmpp channel nick kicked", (SIGNAL_FUNC)sig_nick_kicked);
	signal_remove("xmpp channel disco", (SIGNAL_FUNC)sig_disco);
	signal_remove("xmpp channel joinerror", (SIGNAL_FUNC)sig_joinerror);
	signal_remove("channel created", (SIGNAL_FUNC)sig_channel_created);
	signal_remove("channel destroyed",(SIGNAL_FUNC)sig_channel_destroyed);
	signal_remove("server connected", (SIGNAL_FUNC)sig_server_connected);
}
