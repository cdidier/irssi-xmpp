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

/*
 * XEP-0045: Multi-User Chat
 */

#include <string.h>

#include "module.h"
#include "commands.h"
#include "settings.h"
#include "signals.h"

#include "rosters-tools.h"
#include "tools.h"
#include "disco.h"
#include "muc.h"
#include "muc-commands.h"
#include "muc-events.h"
#include "muc-nicklist.h"
#include "muc-affiliation.h"
#include "muc-role.h"
#include "muc-reconnect.h"

#define XMLNS_DATA_X "jabber:x:data"
#define XMLNS_MUC_CONFIG "http://jabber.org/protocol/muc#roomconfig"

static char *
get_join_data(MUC_REC *channel)
{
	if (channel->key != NULL)
		return g_strdup_printf("\"%s/%s\" \"%s\"",
		    channel->name, channel->nick, channel->key);
	else
		return g_strdup_printf("\"%s/%s\"",
		    channel->name, channel->nick);
}

CHANNEL_REC *
muc_create(XMPP_SERVER_REC *server, const char *name,
    const char *visible_name, int automatic, const char *nick)
{
	MUC_REC *rec;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);
	rec = g_new0(MUC_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;
	rec->nick = g_strdup((nick != NULL) ? nick : 
	  (*settings_get_str("nick") != '\0') ?
	  settings_get_str("nick") : server->user);
	channel_init((CHANNEL_REC *)rec, SERVER(server), name, visible_name,
	    automatic);
	rec->get_join_data = (char *(*)(CHANNEL_REC *))get_join_data;
	return (CHANNEL_REC *)rec;
}

void
muc_nick(MUC_REC *channel, const char *nick)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded, *str;

	g_return_if_fail(IS_MUC(channel));
	if (!channel->server->connected)
		return;
	str = g_strconcat(channel->name, "/", nick, (void *)NULL);
	recoded = xmpp_recode_out(str);
	g_free(str);
	lmsg = lm_message_new(recoded, LM_MESSAGE_TYPE_PRESENCE);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_MUC);
	if (!channel->joined) {
		if (channel->key != NULL) {
			recoded = xmpp_recode_out(channel->key);
			lm_message_node_add_child(node, "password", recoded);
			g_free(recoded);
		}
		node = lm_message_node_add_child(node, "history", NULL);
		str = g_strdup_printf("%d",
		    settings_get_int("xmpp_history_maxstanzas"));
		lm_message_node_set_attribute(node, "maxstanzas", str);
		g_free(str);
		if (channel->server->show != XMPP_PRESENCE_AVAILABLE) {
			recoded = xmpp_recode_out(
			    xmpp_presence_show[channel->server->show]);
			lm_message_node_add_child(lmsg->node, "show", recoded);
			g_free(recoded);
		}
		if (channel->server->away_reason != NULL) {
			recoded = xmpp_recode_out(
			    channel->server->away_reason);
			lm_message_node_add_child(lmsg->node, "status",
			    recoded);
			g_free(recoded);
		}
	}
	signal_emit("xmpp send presence", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
send_join(MUC_REC *channel)
{
	g_return_if_fail(IS_MUC(channel));
	if (!channel->server->connected)
		return;
	muc_nick(channel, channel->nick);
}

void
muc_join(XMPP_SERVER_REC *server, const char *data, gboolean automatic)
{
	MUC_REC *channel;
	char *chanline, *channame, *nick, *key;
	void *free_arg;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(data != NULL);
	if (!server->connected)
		return;
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST,
	    &chanline, &key))
		return;
	nick = muc_extract_nick(chanline);
	channame = muc_extract_channel(chanline);
	if (muc_find(server, channame) == NULL) {
		channel = (MUC_REC *)muc_create(server,
		    channame, NULL, automatic, nick);
		channel->key = (key == NULL || *key == '\0') ?
		    NULL : g_strdup(key);
		send_join(channel);
	}
	g_free(nick);
	g_free(channame);
	cmd_params_free(free_arg);
}

static void
send_part(MUC_REC *channel, const char *reason)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *channame, *recoded;

	if (!channel->server->connected)
		return;
	channame = g_strconcat(channel->name, "/", channel->nick, (void *)NULL);
	recoded = xmpp_recode_out(channame);
	g_free(channame);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_UNAVAILABLE);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_MUC);
	if (reason != NULL) {
		recoded = xmpp_recode_out(reason);
		lm_message_node_add_child(lmsg->node, "status", recoded);
		g_free(recoded);
	}
	signal_emit("xmpp send presence", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_part(MUC_REC *channel, const char *reason)
{
	g_return_if_fail(IS_MUC(channel));
	send_part(channel, reason);
	channel->left = TRUE;
	if (channel->ownnick != NULL)
		signal_emit("message part", 5, channel->server, channel->name,
		    channel->ownnick->nick, channel->ownnick->host, reason);
	channel_destroy(CHANNEL(channel));
}

void
muc_get_affiliation(XMPP_SERVER_REC *server, MUC_REC *channel, const char *type)
{
	LmMessage *lmsg;
	LmMessageNode *query, *item;
	char *recoded;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!channel->server->connected)
		return;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	recoded = xmpp_recode_out(server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_ADMIN);
	item = lm_message_node_add_child(query, "item", NULL);
	recoded = xmpp_recode_out(type);
	lm_message_node_set_attribute(item, "affiliation", recoded);
	g_free(recoded);
	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_set_affiliation(XMPP_SERVER_REC *server, MUC_REC *channel, const char *type,
		const char *jid, const char *reason)
{
	LmMessage *lmsg;
	LmMessageNode *query, *item;
	char *recoded;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!channel->server->connected)
		return;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);
	recoded = xmpp_recode_out(server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_ADMIN);
	item = lm_message_node_add_child(query, "item", NULL);
	recoded = xmpp_recode_out(type);
	lm_message_node_set_attribute(item, "affiliation", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(jid);
	lm_message_node_set_attribute(item, "jid", recoded);
	g_free(recoded);
	if (reason != NULL) {
		recoded = xmpp_recode_out(reason);
		lm_message_node_add_child(item, "reason", recoded);
		g_free(recoded);
	}
	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_get_role(XMPP_SERVER_REC *server, MUC_REC *channel, const char *type)
{
	LmMessage *lmsg;
	LmMessageNode *query, *item;
	char *recoded;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!channel->server->connected)
		return;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	recoded = xmpp_recode_out(server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_ADMIN);
	item = lm_message_node_add_child(query, "item", NULL);
	recoded = xmpp_recode_out(type);
	lm_message_node_set_attribute(item, "role", recoded);
	g_free(recoded);
	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_set_role(XMPP_SERVER_REC *server, MUC_REC *channel, const char *type,
		const char *nick, const char *reason)
{
	LmMessage *lmsg;
	LmMessageNode *query, *item;
	char *recoded;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!channel->server->connected)
		return;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);
	recoded = xmpp_recode_out(server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_ADMIN);
	item = lm_message_node_add_child(query, "item", NULL);
	recoded = xmpp_recode_out(type);
	lm_message_node_set_attribute(item, "role", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(nick);
	lm_message_node_set_attribute(item, "nick", recoded);
	g_free(recoded);
	if (reason != NULL) {
		recoded = xmpp_recode_out(reason);
		lm_message_node_add_child(item, "reason", recoded);
		g_free(recoded);
	}
	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_set_mode(XMPP_SERVER_REC *server ,MUC_REC *channel, const char *mode)
{
	LmMessage *lmsg;
	LmMessageNode *query, *x, *field;
	char *recoded, *new_value, *feature;
	unsigned int i;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);
	recoded = xmpp_recode_out(channel->server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_OWNER);
	x = lm_message_node_add_child(query, "x", NULL);
	lm_message_node_set_attribute(x, XMLNS, XMLNS_DATA_X);
	lm_message_node_set_attribute(x, "type", "submit");

	field = lm_message_node_add_child(x, "field", NULL);
	lm_message_node_set_attribute(field, "var", "FORM_TYPE");
	lm_message_node_add_child(field, "value", XMLNS_MUC_CONFIG);

	new_value = mode[0] == '+' ? "1" : "0";

	for (i = 1; i < strlen(mode); ++i) {
		field = lm_message_node_add_child(x, "field", NULL);
		switch (mode[i]) {
		case 'm':
			feature = "muc#roomconfig_membersonly";
			break;
		case 'M':
			feature = "muc#roomconfig_moderatedroom";
			break;
		case 'k':
			feature = "muc#roomconfig_passwordprotectedroom";
			break;
		case 'p':
			feature = "muc#roomconfig_persistentroom";
			break;
		case 'u':
			feature = "muc#roomconfig_publicroom";
			break;
		default:
			continue;
		}
		lm_message_node_set_attribute(field, "var", feature);
		lm_message_node_add_child(field, "value", new_value);
	}

	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

void
muc_destroy(XMPP_SERVER_REC *server ,MUC_REC *channel, const char *alternate,
		const char *reason)
{
	LmMessage *lmsg;
	LmMessageNode *query, *destroy;
	char *recoded;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!channel->server->connected)
		return;

	lmsg = lm_message_new_with_sub_type(channel->name, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);
	recoded = xmpp_recode_out(server->jid);
	lm_message_node_set_attribute(lmsg->node, "from", recoded);
	g_free(recoded);
	query = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(query, XMLNS, XMLNS_MUC_OWNER);
	destroy = lm_message_node_add_child(query, "destroy", NULL);
	if (alternate != NULL) {
		recoded = xmpp_recode_out(alternate);
		lm_message_node_set_attribute(destroy, "jid", recoded);
		g_free(recoded);
	}
	if (reason != NULL) {
		recoded = xmpp_recode_out(reason);
		lm_message_node_add_child(destroy, "reason", recoded);
		g_free(recoded);
	}
	signal_emit("xmpp send iq", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_features(XMPP_SERVER_REC *server, const char *name, GSList *list)
{
	MUC_REC *channel;
	GString *modes;

	if ((channel = muc_find(server, name)) == NULL)
		return;
	modes = g_string_new(NULL);
	if (disco_have_feature(list, "muc_hidden"))
		g_string_append(modes, "h");
	if (disco_have_feature(list, "muc_membersonly"))
		g_string_append(modes, "m");
	if (disco_have_feature(list, "muc_moderated"))
		g_string_append(modes, "M");
	if (disco_have_feature(list, "muc_nonanonymous"))
		g_string_append(modes, "a");
	if (disco_have_feature(list, "muc_open"))
		g_string_append(modes, "o");
	if (disco_have_feature(list, "muc_passwordprotected"))
		g_string_append(modes, "k");
	if (disco_have_feature(list, "muc_persistent"))
		 g_string_append(modes, "p");
	if (disco_have_feature(list, "muc_public"))
		g_string_append(modes, "u");
	if (disco_have_feature(list, "muc_semianonymous"))
		g_string_append(modes, "b");
	if (disco_have_feature(list, "muc_temporary"))
		g_string_append(modes, "t");
	if (disco_have_feature(list, "muc_unmoderated"))
		g_string_append(modes, "n");
	if (disco_have_feature(list, "muc_unsecured"))
		g_string_append(modes, "d");
	if (disco_have_feature(list, "muc_passwordprotected")
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
sig_channel_created(MUC_REC *channel)
{
	if (!IS_MUC(channel))
		return;
	if (channel->nicks != NULL)
		g_hash_table_destroy(channel->nicks);
	channel->nicks = g_hash_table_new((GHashFunc)g_str_hash,
	    (GCompareFunc)g_str_equal);
}

static void
sig_channel_destroyed(MUC_REC *channel)
{
	if (!IS_MUC(channel))
		return;
	if (!channel->server->disconnected && !channel->left)
		muc_part(channel, settings_get_str("part_message"));
	g_free(channel->nick);
}

static CHANNEL_REC *
channel_find_func(SERVER_REC *server, const char *channel_name)
{
	GSList *tmp;
	CHANNEL_REC *channel;

	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = tmp->data;
		if (channel->chat_type != server->chat_type)
			continue;
		if (g_ascii_strcasecmp(channel_name, channel->name) == 0)
			return channel;
	}
	return NULL;
}

static void
channels_join_func(SERVER_REC *server, const char *data, int automatic)
{
	/* ignore automatic joins from irssi */
	if (automatic)
		return;
	muc_join(XMPP_SERVER(server), data, FALSE);
}

static int
ischannel_func(SERVER_REC *server, const char *data)
{
	char *str;
	gboolean r;

	str = muc_extract_channel(data);
	r = muc_find(server, data) != NULL ? TRUE : FALSE;
	g_free(str);
	return r;
}

MUC_REC *
get_muc(XMPP_SERVER_REC *server, const char *data)
{
	MUC_REC *channel;
	char *str;

	str = muc_extract_channel(data);
	channel = muc_find(server, str);
	g_free(str);
	return channel;
}

static void
sig_connected(SERVER_REC *server)
{
	GSList *tmp;
	CHANNEL_SETUP_REC *channel_setup;

	if (!IS_XMPP_SERVER(server))
		return;
	server->channel_find_func = channel_find_func;
	server->channels_join = channels_join_func;
	server->ischannel = ischannel_func;
	/* autojoin channels */
	if (!server->connrec->no_autojoin_channels) {
		for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
			channel_setup = tmp->data;
			if (IS_MUC_SETUP(channel_setup)
			    && channel_setup->autojoin
			    && strcmp(channel_setup->chatnet,
			    server->connrec->chatnet) == 0)
				 muc_join(XMPP_SERVER(server),
				     channel_setup->name, TRUE);
		}
	}
}

static void
send_muc_presence(MUC_REC *channel, const int show, const char *status)
{
	LmMessage *lmsg;
	char *channame, *str;

	channame = g_strconcat(channel->name, "/", channel->nick, (void *)NULL);
	str = xmpp_recode_out(channame);
	g_free(channame);
	lmsg = lm_message_new(str, LM_MESSAGE_TYPE_PRESENCE);
	g_free(str);
	if (show != XMPP_PRESENCE_AVAILABLE)
		lm_message_node_add_child(lmsg->node, "show",
		    xmpp_presence_show[show]);
	if (status != NULL) {
		str = xmpp_recode_out(status);
		lm_message_node_add_child(lmsg->node, "status", str);
		g_free(str);
	}
	signal_emit("xmpp send presence", 2, channel->server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_set_presence(XMPP_SERVER_REC *server, const int show, const char *status,
    const int priority)
{
	GSList *tmp;
	MUC_REC *channel;

	g_return_if_fail(IS_XMPP_SERVER(server));
	if (!server->connected)
		return;
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = MUC(tmp->data);
		send_muc_presence(channel, show, status);
	}
}

void
muc_init(void)
{
	CHAT_PROTOCOL_REC *chat;

	if ((chat = chat_protocol_find(XMPP_PROTOCOL_NAME)) != NULL)
		chat->channel_create = (CHANNEL_REC *(*)
		    (SERVER_REC *, const char *, const char *, int))muc_create;

	disco_add_feature(XMLNS_MUC);
	muc_commands_init();
	muc_events_init();
	muc_nicklist_init();
	muc_reconnect_init();

	signal_add("xmpp features", sig_features);
	signal_add("channel created", sig_channel_created);
	signal_add("channel destroyed", sig_channel_destroyed);
	signal_add("server connected", sig_connected);
	signal_add("xmpp set presence", sig_set_presence);

	settings_add_int("xmpp_lookandfeel", "xmpp_history_maxstanzas", 30);
}

void
muc_deinit(void)
{
	signal_remove("xmpp features", sig_features);
	signal_remove("channel created", sig_channel_created);
	signal_remove("channel destroyed",sig_channel_destroyed);
	signal_remove("server connected", sig_connected);
	signal_remove("xmpp set presence", sig_set_presence);

	muc_commands_deinit();
	muc_events_deinit();
	muc_nicklist_deinit();
	muc_reconnect_deinit();
}
