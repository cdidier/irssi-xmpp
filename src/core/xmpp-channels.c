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
#include "channels-setup.h"
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

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);

	rec = g_new0(XMPP_CHANNEL_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;

	rec->nick = g_strdup((nick != NULL) ? nick :
	  (*settings_get_str("nick") != '\0') ?
	  settings_get_str("nick") : server->user);
	rec->features = 0;

	channel_init((CHANNEL_REC *)rec, SERVER(server), name, visible_name,
	    automatic);

	return (CHANNEL_REC *)rec;
}

void
xmpp_channel_send_message(XMPP_SERVER_REC *server, const char *name,
    const char *message)
{
	LmMessage *msg;
	char *room, *message_recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
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
send_nick(XMPP_CHANNEL_REC *channel, const char *nick)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room, *recoded, *str;

	g_return_if_fail(channel != NULL);
	g_return_if_fail(IS_XMPP_SERVER(channel->server));

	if (!channel->server->connected)
		return;

	room = g_strconcat(channel->name, "/", nick, NULL);
	recoded = xmpp_recode_out(room);
	g_free(room);

	msg = lm_message_new(recoded, LM_MESSAGE_TYPE_PRESENCE);
	g_free(recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_MUC);

	if (!channel->joined) {
		if (channel->key != NULL) {
			recoded = xmpp_recode_out(channel->key);
			lm_message_node_add_child(child, "password", recoded);
			g_free(recoded);
		}

		child = lm_message_node_add_child(child, "history", NULL);
		str = g_strdup_printf("%d",
		    settings_get_int("xmpp_history_maxstanzas"));
		lm_message_node_set_attribute(child, "maxstanzas", str);
		g_free(str);

		if (channel->server->show != XMPP_PRESENCE_AVAILABLE) {
			recoded = xmpp_recode_out(
			    xmpp_presence_show[channel->server->show]);
			lm_message_node_add_child(msg->node, "show", recoded);
			g_free(recoded);
		}

		if (channel->server->away_reason != NULL) {
			recoded = xmpp_recode_out(
			    channel->server->away_reason);
			lm_message_node_add_child(msg->node, "status",
			    recoded);
			g_free(recoded);
		}
	}

	lm_send(channel->server, msg, NULL);
	lm_message_unref(msg);
}

static void
send_disco(XMPP_CHANNEL_REC *channel)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room_recoded;

	g_return_if_fail(channel != NULL);
	g_return_if_fail(IS_XMPP_SERVER(channel->server));

	room_recoded = xmpp_recode_out(channel->name);
	msg = lm_message_new_with_sub_type(room_recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(room_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_DISCO_INFO);

	lm_send(channel->server, msg, NULL);
	lm_message_unref(msg);
}

static void
send_join(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(channel != NULL);

	if (!server->connected)
		return;

	send_disco(channel);
	send_nick(channel, channel->nick);
}

static void
channel_join(XMPP_SERVER_REC *server, const char *data, int automatic)
{
	XMPP_CHANNEL_REC *channel;
	char *chanline, *channame, *nick, *key;
	void *free_arg;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(data != NULL);

	g_strstrip((char *)data);
	if (!server->connected || *data == '\0')
		return;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST,
	    &chanline, &key))
		return;

	nick = xmpp_extract_nick(chanline);
	channame = xmpp_extract_channel(chanline);

	if (xmpp_channel_find(server, channame) == NULL) {
		channel = (XMPP_CHANNEL_REC *)xmpp_channel_create(server,
		    channame, NULL, automatic, nick);

		channel->key = (key == NULL || *key == '\0') ?
		    NULL : g_strdup(key);
		send_join(server, channel);
	}

	g_free(nick);
	g_free(channame);
	cmd_params_free(free_arg);
}

void
xmpp_channels_join(XMPP_SERVER_REC *server, const char *data, int automatic)
{
	/* ignore automatic joins from irssi */
	if (automatic)
		return;

	channel_join(server, data, FALSE);
}

void
xmpp_channels_join_automatic(XMPP_SERVER_REC *server, const char *data)
{
	channel_join(server, data, TRUE);
}

static void
send_part(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *reason)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *room, *room_recoded, *reason_recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(channel != NULL);

	if (!server->connected)
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
sig_part(XMPP_SERVER_REC *server, const char *channame,
    const char *reason)
{
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(channame != NULL);

	g_strstrip((char *)channame);
	channel = xmpp_channel_find(server, channame);
	if (channel != NULL) {
		send_part(server, channel, reason);
		channel->left = TRUE;

		if (channel->ownnick != NULL)
			signal_emit("message part", 5, server,
			    channel->name, channel->ownnick->nick,
			    channel->ownnick->host, reason);

		channel_destroy(CHANNEL(channel));
	}
}

static void
sig_own_nick(XMPP_SERVER_REC *server, const char *channame, const char *nick)
{
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(channame != NULL);
	g_return_if_fail(nick != NULL);

	g_strstrip((char *)channame);
	channel = xmpp_channel_find(server, channame);
	if (channel != NULL)
		send_nick(channel, nick);
}

static void
sig_topic(XMPP_CHANNEL_REC *channel, const char *topic, const char *nick_name)
{
	g_return_if_fail(channel != NULL);

	if (channel->topic != NULL && topic != NULL
	    && strcmp(channel->topic, topic) == 0)
		return;

	g_free(channel->topic);
	channel->topic = (topic != NULL && *topic != '\0') ?
	    g_strdup(topic) : NULL;

	g_free(channel->topic_by);
	channel->topic_by = g_strdup(nick_name);

	signal_emit("channel topic changed", 1, channel);

	if (channel->joined && nick_name != NULL && *nick_name != '\0')
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
sig_nick_own_event(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *full_jid, const char *affiliation, const char *role,
    gboolean forced)
{
	XMPP_NICK_REC *nick;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(nick_name != NULL);

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick == NULL)
		signal_emit("xmpp channel nick own_join", 6, channel,
		    nick_name, full_jid, affiliation, role, forced);
	else
		signal_emit("xmpp channel nick mode", 4, channel, nick,
		    affiliation, role);
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
		signal_emit("xmpp channel nick join", 5, channel, nick_name,
		    full_jid, affiliation, role);
	else 
		signal_emit("xmpp channel nick mode", 4, channel, nick,
		    affiliation, role);
}

static void
sig_nick_own_join(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *full_jid, const char *affiliation, const char *role,
    gboolean forced)
{
	XMPP_NICK_REC *nick;

	if (channel->joined)
		return;

	nick = xmpp_nicklist_find(channel, nick_name);
	if (nick != NULL)
		return;

	nick = xmpp_nicklist_insert(channel, nick_name, full_jid);
	nicklist_set_own(CHANNEL(channel), NICK(nick));
	channel->chanop = channel->ownnick->op;
	xmpp_nicklist_set_modes(nick,
	    xmpp_nicklist_get_affiliation(affiliation),
	    xmpp_nicklist_get_role(role));

	channel->names_got = TRUE;
	channel->joined = TRUE;

	signal_emit("message join", 4, channel->server, channel->name,
	    nick->nick, nick->host);
	signal_emit("message xmpp channel mode", 4, channel,
	    nick->nick, nick->affiliation, nick->role);

	signal_emit("channel joined", 1, channel);
	signal_emit("channel sync", 1, channel);
	channel_send_autocommands(CHANNEL(channel));

	if (forced)
		signal_emit("xmpp channel nick", 3, channel, channel->nick,
		    nick->nick);

	if (*channel->mode == '\0')
		send_disco(channel);
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

	if (channel->names_got) {
		signal_emit("message join", 4, channel->server, channel->name,
		    nick->nick, nick->host);
		signal_emit("message xmpp channel mode", 4, channel,
		    nick->nick, nick->affiliation, nick->role);
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

	if (channel->ownnick == NICK(nick)) {
		channel->left = TRUE;
		channel_destroy(CHANNEL(channel));
	} else
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

		signal_emit("message xmpp channel mode", 4, channel,
		    nick->nick, affiliation, role);
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
			/* TODO show event */
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
		signal_emit("message xmpp channel own_nick", 3,
		    channel, nick, oldnick);
	else
		signal_emit("message xmpp channel nick", 3,
		    channel, nick, oldnick);

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

	if (!(features & XMPP_CHANNELS_FEATURE_HIDDEN)
	    && g_ascii_strcasecmp(var, "muc_hidden") == 0)
		return XMPP_CHANNELS_FEATURE_HIDDEN;

	if (!(features & XMPP_CHANNELS_FEATURE_MEMBERS_ONLY)
	     && g_ascii_strcasecmp(var, "muc_membersonly") == 0)
		return XMPP_CHANNELS_FEATURE_MEMBERS_ONLY;

	if (!(features & XMPP_CHANNELS_FEATURE_MODERATED)
	    && g_ascii_strcasecmp(var, "muc_moderated") == 0)
		return XMPP_CHANNELS_FEATURE_MODERATED;

	if (!(features & XMPP_CHANNELS_FEATURE_NONANONYMOUS)
	    && g_ascii_strcasecmp(var, "muc_nonanonymous") == 0)
		return XMPP_CHANNELS_FEATURE_NONANONYMOUS;

	if (!(features & XMPP_CHANNELS_FEATURE_OPEN)
	    && g_ascii_strcasecmp(var, "muc_open") == 0)
		return XMPP_CHANNELS_FEATURE_OPEN;

	if (!(features & XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED)
	    && g_ascii_strcasecmp(var, "muc_passwordprotected") == 0)
		return XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED;

	if (!(features & XMPP_CHANNELS_FEATURE_PERSISTENT)
	    && g_ascii_strcasecmp(var, "muc_persistent") == 0)
		return XMPP_CHANNELS_FEATURE_PERSISTENT;

	if (!(features & XMPP_CHANNELS_FEATURE_PUBLIC)
	    && g_ascii_strcasecmp(var, "muc_public") == 0)
		return XMPP_CHANNELS_FEATURE_PUBLIC;

	if (!(features & XMPP_CHANNELS_FEATURE_SEMIANONYMOUS)
	    && g_ascii_strcasecmp(var, "muc_semianonymous") == 0)
		return XMPP_CHANNELS_FEATURE_SEMIANONYMOUS;

	if (!(features & XMPP_CHANNELS_FEATURE_TEMPORARY)
	    && g_ascii_strcasecmp(var, "muc_temporary") == 0)
		return XMPP_CHANNELS_FEATURE_TEMPORARY;

	if (!(features & XMPP_CHANNELS_FEATURE_UNMODERATED)
	    && g_ascii_strcasecmp(var, "muc_unmoderated") == 0)
		return XMPP_CHANNELS_FEATURE_UNMODERATED;

	if (!(features & XMPP_CHANNELS_FEATURE_UNSECURED)
	    && g_ascii_strcasecmp(var, "muc_unsecured") == 0)
		return XMPP_CHANNELS_FEATURE_UNSECURED;

	else
		return 0;
}

static void
update_channel_modes(XMPP_CHANNEL_REC *channel)
{
	GString *modes;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));

	modes = g_string_new(NULL);

	if (channel->features & XMPP_CHANNELS_FEATURE_HIDDEN)
		g_string_append(modes, "h");
	if (channel->features & XMPP_CHANNELS_FEATURE_MEMBERS_ONLY)
		g_string_append(modes, "m");
	if (channel->features & XMPP_CHANNELS_FEATURE_MODERATED)
		g_string_append(modes, "M");
	if (channel->features & XMPP_CHANNELS_FEATURE_NONANONYMOUS)
		g_string_append(modes, "a");
	if (channel->features & XMPP_CHANNELS_FEATURE_OPEN)
		g_string_append(modes, "o");
	if (channel->features & XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED)
		g_string_append(modes, "k");
	if (channel->features & XMPP_CHANNELS_FEATURE_PERSISTENT)
		g_string_append(modes, "p");
	if (channel->features & XMPP_CHANNELS_FEATURE_PUBLIC)
		g_string_append(modes, "u");
	if (channel->features & XMPP_CHANNELS_FEATURE_SEMIANONYMOUS)
		g_string_append(modes, "b");
	if (channel->features & XMPP_CHANNELS_FEATURE_TEMPORARY)
		g_string_append(modes, "t");
	if (channel->features & XMPP_CHANNELS_FEATURE_UNMODERATED)
		g_string_append(modes, "n");
	if (channel->features & XMPP_CHANNELS_FEATURE_UNSECURED)
		g_string_append(modes, "d");

	if (channel->features & XMPP_CHANNELS_FEATURE_PASSWORD_PROTECTED
	    && channel->key != NULL)
		g_string_append_printf(modes, " %s", channel->key);

	if (strcmp(modes->str, channel->mode) != 0) {
		g_free(channel->mode);
		channel->mode = modes->str;

		signal_emit("channel mode changed", 2, channel, channel->name);
	}

	g_string_free(modes, FALSE);
}

static void
sig_disco(XMPP_CHANNEL_REC *channel, LmMessageNode *query)
{
	LmMessageNode *item;
	const char *var;

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
	g_return_if_fail(query != NULL);

	channel->features = 0;
	for (item = query->children; item != NULL; item = item->next) {
		if (g_ascii_strcasecmp(item->name, "feature") != 0)
			continue;

		/* <feature var='var'/> */
		var = lm_message_node_get_attribute(item, "var");
		if (var != NULL)
			channel->features |= disco_parse_channels_features(var,
			    channel->features);
	}
	update_channel_modes(channel);
}

static void
sig_joinerror(XMPP_CHANNEL_REC *channel, int error)
{
	g_return_if_fail(IS_XMPP_CHANNEL(channel));

	/* retry with alternate nick */
	if (error == XMPP_CHANNELS_ERROR_USE_RESERVED_ROOM_NICK
	    || error == XMPP_CHANNELS_ERROR_NICK_IN_USE) {
		const char *altnick = settings_get_str("alternate_nick");

		if (altnick != NULL && *altnick != '\0'
		    && strcmp(channel->nick, altnick) != 0) {
			g_free(channel->nick);
			channel->nick = g_strdup(altnick);
		} else {
			char *str = g_strdup_printf("%s_", channel->nick);
			g_free(channel->nick);
			channel->nick = str;
		}

		send_join(channel->server, channel);
		return;
	}

	channel_destroy(CHANNEL(channel));
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
sig_event_connected(SERVER_REC *server)
{
	GSList *tmp;
	CHANNEL_SETUP_REC *channel_setup;

	if (!IS_XMPP_SERVER(server)
	    || server->connrec->no_autojoin_channels)
		return;

	/* autojoin channels */
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		channel_setup = tmp->data;

		if (IS_XMPP_CHANNEL_SETUP(channel_setup)
		    && channel_setup->autojoin
		    && strcmp(channel_setup->chatnet,
		    server->connrec->chatnet) == 0)
		    	xmpp_channels_join_automatic(XMPP_SERVER(server),
			    channel_setup->name);
	}
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
	signal_add("xmpp channel nick own_event",
	    (SIGNAL_FUNC)sig_nick_own_event);
	signal_add("xmpp channel nick event", (SIGNAL_FUNC)sig_nick_event);
	signal_add("xmpp channel nick own_join",
	    (SIGNAL_FUNC)sig_nick_own_join);
	signal_add("xmpp channel nick join", (SIGNAL_FUNC)sig_nick_join);
	signal_add("xmpp channel nick part", (SIGNAL_FUNC)sig_nick_part);
	signal_add("xmpp channel nick mode", (SIGNAL_FUNC)sig_nick_mode);
	signal_add("xmpp channel nick presence",
	    (SIGNAL_FUNC)sig_nick_presence);
	signal_add("xmpp channel nick", (SIGNAL_FUNC)sig_nick_changed);
	signal_add("xmpp channel nick kicked", (SIGNAL_FUNC)sig_nick_kicked);
	signal_add("xmpp channel disco", (SIGNAL_FUNC)sig_disco);
	signal_add_last("xmpp channel joinerror", (SIGNAL_FUNC)sig_joinerror);
	signal_add_last("channel created", (SIGNAL_FUNC)sig_channel_created);
	signal_add("channel destroyed", (SIGNAL_FUNC)sig_channel_destroyed);
	signal_add_last("event connected", (SIGNAL_FUNC)sig_event_connected);
	signal_add_first("server connected",
	    (SIGNAL_FUNC)sig_server_connected);

	settings_add_int("xmpp_lookandfeel", "xmpp_history_maxstanzas", 30);
}

void
xmpp_channels_deinit(void)
{
	signal_remove("xmpp channels part", (SIGNAL_FUNC)sig_part);
	signal_remove("xmpp channels own_nick", (SIGNAL_FUNC)sig_own_nick);
	signal_remove("xmpp channel topic", (SIGNAL_FUNC)sig_topic);
	signal_remove("xmpp channel nick own_event",
	    (SIGNAL_FUNC)sig_nick_own_event);
	signal_remove("xmpp channel nick event", (SIGNAL_FUNC)sig_nick_event);
	signal_remove("xmpp channel nick own_join",
	    (SIGNAL_FUNC)sig_nick_own_join);
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
	signal_remove("event connected", (SIGNAL_FUNC)sig_event_connected);
	signal_remove("server connected", (SIGNAL_FUNC)sig_server_connected);
}
