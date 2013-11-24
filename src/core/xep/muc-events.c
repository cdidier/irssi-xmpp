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

#include <stdlib.h>
#include <string.h>

#include "module.h"
#include "commands.h"
#include "misc.h"
#include "settings.h"
#include "signals.h"

#include "rosters-tools.h"
#include "tools.h"
#include "disco.h"
#include "muc.h"
#include "muc-nicklist.h"

#define MAX_LONG_STRLEN ((sizeof(long) * CHAR_BIT + 2) / 3 + 1)

void send_join(MUC_REC *);

static void
topic(MUC_REC *channel, const char *topic, const char *nickname)
{
	if (channel->topic != NULL && topic != NULL
	    && strcmp(channel->topic, topic) == 0)
		return;
	g_free(channel->topic);
	channel->topic = (topic != NULL && *topic != '\0') ?
	    g_strdup(topic) : NULL;
	g_free(channel->topic_by);
	channel->topic_by = g_strdup(nickname);
	signal_emit("channel topic changed", 1, channel);
	if (channel->joined && nickname != NULL && *nickname != '\0')
		signal_emit("message topic", 5, channel->server, channel->name,
		    (channel->topic != NULL) ? channel->topic : "",
		    channel->topic_by, "");
	else {
		char *data = g_strconcat(" ", channel->name, " :",
		    (channel->topic != NULL) ? channel->topic : "", (void *)NULL);
		signal_emit("event 332", 2, channel->server, data);
		g_free(data);
	}
}

static void
nick_changed(MUC_REC *channel, const char *oldnick, const char *newnick)
{
	XMPP_NICK_REC *nick;

	if ((nick = xmpp_nicklist_find(channel, oldnick)) == NULL)
		return;
	xmpp_nicklist_rename(channel, nick, oldnick, newnick);
	if (channel->ownnick == NICK(nick))
		signal_emit("message xmpp muc own_nick", 3,
		    channel, nick, oldnick);
	else
		signal_emit("message xmpp muc nick", 3,
		    channel, nick, oldnick);
}

static void
own_join(MUC_REC *channel, const char *nickname,
    const char *full_jid, const char *affiliation, const char *role,
    gboolean forced)
{
	XMPP_NICK_REC *nick;

	if (channel->joined)
		return;
	if ((nick = xmpp_nicklist_find(channel, nickname)) != NULL)
		return;
	nick = xmpp_nicklist_insert(channel, nickname, full_jid);
	nicklist_set_own(CHANNEL(channel), NICK(nick));
	channel->chanop = channel->ownnick->op;
	xmpp_nicklist_set_modes(nick,
	    xmpp_nicklist_get_affiliation(affiliation),
	    xmpp_nicklist_get_role(role));
	channel->names_got = TRUE;
	channel->joined = TRUE;
	signal_emit("message join", 4, channel->server, channel->name,
	    nick->nick, nick->host);
	signal_emit("message xmpp muc mode", 4, channel,
	    nick->nick, nick->affiliation, nick->role);
	signal_emit("channel joined", 1, channel);
	signal_emit("channel sync", 1, channel);
	channel_send_autocommands(CHANNEL(channel));
	if (forced)
		nick_changed(channel, channel->nick, nick->nick);
	if (*channel->mode == '\0')
		disco_request(channel->server, channel->name);
}

static void
nick_join(MUC_REC *channel, const char *nickname, const char *full_jid,
    const char *affiliation, const char *role)
{
	XMPP_NICK_REC *nick;

	nick = xmpp_nicklist_insert(channel, nickname, full_jid);
	xmpp_nicklist_set_modes(nick,
	    xmpp_nicklist_get_affiliation(affiliation),
	    xmpp_nicklist_get_role(role));
	if (channel->names_got) {
		signal_emit("message join", 4, channel->server, channel->name,
		    nick->nick, nick->host);
		signal_emit("message xmpp muc mode", 4, channel,
		    nick->nick, nick->affiliation, nick->role);
	}
}

static void
nick_mode(MUC_REC *channel, XMPP_NICK_REC *nick, const char *affiliation_str,
    const char *role_str)
{
	int affiliation, role;

	affiliation = xmpp_nicklist_get_affiliation(affiliation_str);
	role = xmpp_nicklist_get_role(role_str);
	if (xmpp_nicklist_modes_changed(nick, affiliation, role)) {
		xmpp_nicklist_set_modes(nick, affiliation, role);
		signal_emit("message xmpp muc mode", 4, channel,
		    nick->nick, affiliation, role);
	}
}

static void
own_event(MUC_REC *channel, const char *nickname, const char *full_jid, 
    const char *affiliation, const char *role, gboolean forced)
{
	XMPP_NICK_REC *nick;

	if ((nick = xmpp_nicklist_find(channel, nickname)) == NULL)
		own_join(channel, nickname, full_jid, affiliation, role,
		    forced);
	else
		nick_mode(channel, nick, affiliation, role);
}

static void
nick_event(MUC_REC *channel, const char *nickname, const char *full_jid,
    const char *affiliation, const char *role)
{
	XMPP_NICK_REC *nick;

	if ((nick = xmpp_nicklist_find(channel, nickname)) == NULL)
		nick_join(channel, nickname, full_jid, affiliation, role);
	else 
		nick_mode(channel, nick, affiliation, role);
}

static void
nick_part(MUC_REC *channel, const char *nickname, const char *reason)
{
	XMPP_NICK_REC *nick;

	if ((nick = xmpp_nicklist_find(channel, nickname)) == NULL)
		return;
	signal_emit("message part", 5, channel->server, channel->name,
	    nick->nick, nick->host, reason);
	if (channel->ownnick == NICK(nick)) {
		channel->left = TRUE;
		channel_destroy(CHANNEL(channel));
	} else
		nicklist_remove(CHANNEL(channel), NICK(nick));
}

static void
nick_presence(MUC_REC *channel, const char *nickname, const char *show_str,
    const char *status)
{
	XMPP_NICK_REC *nick;
	int show;

	if ((nick = xmpp_nicklist_find(channel, nickname)) == NULL)
		return;
	show = xmpp_get_show(show_str);
	if (xmpp_presence_changed(show, nick->show, status, nick->status,
	    0, 0)) {
		xmpp_nicklist_set_presence(nick, show, status);
		if (channel->joined && channel->ownnick != NICK(nick)) {
			/* TODO eventually show event */
		}
	}
}

static void
nick_kicked(MUC_REC *channel, const char *nickname, const char *actor,
    const char *reason)
{
	XMPP_NICK_REC *nick;

	if ((nick = xmpp_nicklist_find(channel, nickname)) == NULL)
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

static void
error_message(MUC_REC *channel, const char *code)
{
	int error;

	error = code != NULL ? atoi(code) : MUC_ERROR_UNKNOWN;
	switch (error) {
	case MUC_ERROR_PASSWORD_INVALID_OR_MISSING:
		signal_emit("xmpp muc error", 2, channel, "not allowed");
		break;
	}
}

static void
error_join(MUC_REC *channel, const char *code, const char *nick)
{
	char *altnick;
	int error;

	if (nick != NULL && strcmp(nick, channel->nick) != 0)
		return;
	error = code != NULL ? atoi(code) : MUC_ERROR_UNKNOWN;
	signal_emit("xmpp muc joinerror", 2, channel, GINT_TO_POINTER(error));
	switch(error) {
	case MUC_ERROR_USE_RESERVED_ROOM_NICK:
	case MUC_ERROR_NICK_IN_USE:
		/* rejoin with alternate nick */
		altnick = (char *)settings_get_str("alternate_nick");
		if (altnick != NULL && *altnick != '\0'
		    && strcmp(channel->nick, altnick) != 0) {
			g_free(channel->nick);
			channel->nick = g_strdup(altnick);
		} else {
			altnick = g_strdup_printf("%s_", channel->nick);
			g_free(channel->nick);
			channel->nick = altnick;
		}
		send_join(channel);
		return; /* don't destroy the channel */
	}
	channel_destroy(CHANNEL(channel));
}

static void
error_presence(MUC_REC *channel, const char *code, const char *nick)
{
	int error;

	error = code != NULL ? atoi(code) : MUC_ERROR_UNKNOWN;
	switch (error) {
	case MUC_ERROR_NICK_IN_USE:
		signal_emit("message xmpp muc nick in use", 2, channel, nick);
		break;
	}
}

static void
error_destroy(MUC_REC *channel, const char *code, const char *reason)
{
	int error;

	error = code != NULL ? atoi(code) : MUC_ERROR_UNKNOWN;
	switch (error) {
	case 403:
		signal_emit("xmpp muc destroyerror", 2, channel, reason);
		break;
	}
}

static void
available(MUC_REC *channel, const char *from, LmMessage *lmsg)
{
	LmMessageNode *node;
	const char *item_affiliation, *item_role, *nick;
	char *item_jid, *item_nick, *status;
	gboolean own, forced, created;

	item_affiliation = item_role = status = NULL;
	item_jid = item_nick = NULL;
	/* <x xmlns='http://jabber.org/protocol/muc#user'> */
	if ((node = lm_find_node(lmsg->node, "x", XMLNS,
	    XMLNS_MUC_USER)) == NULL)
		return;
	/* <status code='110'/> */
	own = lm_find_node(node, "status", "code", "110") != NULL;
	/* <status code='210'/> */
	forced = lm_find_node(node, "status", "code", "210") != NULL;
	/* <status code='201'/> */
	created = lm_find_node(node, "status", "code", "201") != NULL;
	if (created) {
		char str[MAX_LONG_STRLEN], *data;

		g_snprintf(str, sizeof(str), "%ld", (long)time(NULL));
		data = g_strconcat("_ ", channel->name, " ", str, (void *)NULL);
		/* muc created */
		signal_emit("event 329", 2, channel->server, data);
		g_free(data);
	}
	if ((node = lm_message_node_get_child(node, "item")) == NULL)
		return;
	/* <item affiliation='item_affiliation'
	 *     role='item_role'
	 *     jid='item_jid'
	 *     nick='item_nick'/> */
	item_affiliation = lm_message_node_get_attribute(node, "affiliation");
	item_role = lm_message_node_get_attribute(node, "role");
	item_jid = xmpp_recode_in( lm_message_node_get_attribute(node, "jid"));
	item_nick = xmpp_recode_in( lm_message_node_get_attribute(node, "nick"));
	nick = item_nick != NULL ? item_nick : from;
	if (nick == NULL)
		goto err;
	if (own || strcmp(nick, channel->nick) == 0)
		own_event(channel, nick, item_jid, item_affiliation, item_role,
		    forced);
	else 
		nick_event(channel, nick, item_jid, item_affiliation, item_role);
	/* <status>text</status> */
	node = lm_message_node_get_child(lmsg->node, "status");
	if (node != NULL)
		status = xmpp_recode_in(node->value);
	/* <show>show</show> */
	node = lm_message_node_get_child(lmsg->node, "show");
	nick_presence(channel, nick, node != NULL ? node->value : NULL, status);
	g_free(status);
err:	g_free(item_jid);
	g_free(item_nick);
}

static void
unavailable(MUC_REC *channel, const char *nick, LmMessage *lmsg)
{
	LmMessageNode *node, *child;
	const char *status_code;
	char *reason, *actor, *item_nick, *status;

	status_code = NULL;
	reason = actor = item_nick = status = NULL;
	/* <x xmlns='http://jabber.org/protocol/muc#user'> */
	node = lm_find_node(lmsg->node, "x", XMLNS, XMLNS_MUC_USER);
	if (node != NULL) {
		/* <status code='status_code'/> */
		child = lm_message_node_get_child(node, "status");
		if (child != NULL)
			status_code =
			    lm_message_node_get_attribute(child, "code");
		/* <item nick='item_nick'> */
		node = lm_message_node_get_child(node, "item");
		if (node != NULL) {
			item_nick = xmpp_recode_in(
			    lm_message_node_get_attribute(node, "nick"));
			/* <reason>reason</reason> */
			child = lm_message_node_get_child(node, "reason");
			if (child != NULL)
				reason = xmpp_recode_in(child->value);
			/* <actor jid='actor'/> */
			child = lm_message_node_get_child(node, "actor");
			if (child != NULL)
				actor = xmpp_recode_in(
				    lm_message_node_get_attribute(child, "jid"));
		}
	}
	if (status_code != NULL) {
		switch (atoi(status_code)) {
		case 303: /* <status code='303'/> */
			nick_changed(channel, nick, item_nick);
			break;
		case 307: /* kick: <status code='307'/> */
			nick_kicked(channel, nick, actor, reason);
			break;
		case 301: /* ban: <status code='301'/> */
			nick_kicked(channel, nick, actor, reason);
			break;
		}
	} else {
		/* <status>text</status> */
		node = lm_message_node_get_child(lmsg->node, "status");
		if (node != NULL)
			status = xmpp_recode_in(node->value);
		nick_part(channel, nick, status);
		g_free(status);
	}
	g_free(item_nick);
	g_free(reason);
	g_free(actor);
}

static void
invite(XMPP_SERVER_REC *server, const char *channame, LmMessageNode *node,
    LmMessageNode *invite_node)
{
	LmMessageNode *pass;
	CHANNEL_SETUP_REC *setup;
	const char *from;
	char *inviter, *password, *joindata;

	if ((from = lm_message_node_get_attribute(invite_node, "from")) == NULL)
		return;
	inviter = xmpp_recode_in(from);
	pass = lm_message_node_get_child(node, "password");
	password = pass != NULL ? xmpp_recode_in(pass->value) : NULL;
	signal_emit("xmpp invite", 4, server, inviter, channame, password);
	/* check if we're supposed to autojoin this muc */
	setup = channel_setup_find(channame, server->connrec->chatnet);
	if (setup != NULL && setup->autojoin
	    && settings_get_bool("join_auto_chans_on_invite")) {
		joindata = password == NULL ?
		    g_strconcat("\"", channame, "\"", (void *)NULL)
		    : g_strconcat("\"", channame, "\" ",
			password, (void *)NULL);
		muc_join(server, joindata, TRUE);
		g_free(joindata);
	}
	g_free(inviter);
	g_free(password);
	g_free(server->last_invite);
	server->last_invite = g_strdup(channame);
}

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	MUC_REC *channel;
	LmMessageNode *node, *child;
	char *nick, *str;
	gboolean action, own;

	if ((channel = get_muc(server, from)) == NULL) {
		/* Not a joined channel, search the MUC namespace */
		/* <x xmlns='http://jabber.org/protocol/muc#user'> */
		node = lm_find_node(lmsg->node, "x", XMLNS, XMLNS_MUC_USER);
		if (node == NULL)
			return;
		switch (type) {
		case LM_MESSAGE_SUB_TYPE_NOT_SET:
		case LM_MESSAGE_SUB_TYPE_NORMAL:
			child = lm_message_node_get_child(node, "invite");
			if (child != NULL)
				invite(server, from, node, child);
			break;
		}
		return;
	}
	nick = muc_extract_nick(from);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_ERROR:
		node = lm_message_node_get_child(lmsg->node, "error");
		if (node != NULL) {
			/* TODO: extract error type and name -> XMLNS_STANZAS */
			error_message(channel,
			    lm_message_node_get_attribute(node, "code"));
		}
		break;
	case LM_MESSAGE_SUB_TYPE_GROUPCHAT:
		node = lm_message_node_get_child(lmsg->node, "subject");
		if (node != NULL) {
			str = xmpp_recode_in(node->value);
			topic(channel, str, nick);
			g_free(str);
		}
		node = lm_message_node_get_child(lmsg->node, "body");
		if (node != NULL && node->value != NULL && nick != NULL) {
			str = xmpp_recode_in(node->value);
			own = strcmp(nick, channel->nick) == 0;
			action = g_ascii_strncasecmp(str, "/me ", 4) == 0;
			if (action && own)
				signal_emit("message xmpp own_action", 4,
				    server, str+4, channel->name,
				    GINT_TO_POINTER(SEND_TARGET_CHANNEL));
			else if (action)
				signal_emit("message xmpp action", 5,
				    server, str+4, nick, channel->name,
				    GINT_TO_POINTER(SEND_TARGET_CHANNEL));
			else if (own)
				signal_emit("message xmpp own_public", 3,
				    server, str, channel->name);
			else
				signal_emit("message public", 5,
				    server, str, nick, "", channel->name);
			g_free(str);
		}
		break;
	}
	g_free(nick);
}

static void
sig_recv_presence(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	MUC_REC *channel;
	LmMessageNode *node;
	const char *code;
	char *nick;

	if ((channel = get_muc(server, from)) == NULL)
		return;
	nick = muc_extract_nick(from);
	switch (type) {
	case LM_MESSAGE_SUB_TYPE_ERROR:
		node = lm_message_node_get_child(lmsg->node, "error");
		if (node == NULL)
			goto out;
		/* TODO: extract error type and name -> XMLNS_STANZAS */
		code = lm_message_node_get_attribute(node, "code");
		if (!channel->joined)
			error_join(channel, code, nick);
		else
			error_presence(channel, code, nick);
		break;
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		available(channel, nick, lmsg);
		break;
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		unavailable(channel, nick, lmsg);
		break;
	}

out:
	g_free(nick);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	MUC_REC *channel;
	LmMessageNode *node, *error, *text, *query;
	const char *code;
	char *reason = NULL;

	if ((channel = get_muc(server, from)) == NULL)
		goto out;

	switch (type) {
	case LM_MESSAGE_SUB_TYPE_ERROR:
		error = lm_message_node_get_child(lmsg->node, "error");
		if (error == NULL)
			goto out;
		code = lm_message_node_get_attribute(error, "code");

		query = lm_find_node(lmsg->node, "query", XMLNS, XMLNS_MUC_OWNER);
		if (query == NULL)
			goto out;
		for (node = query->children; node != NULL; node = node->next) {
			if (strcmp(node->name, "destroy") == 0) {
				text = lm_message_node_get_child(error, "text");
				reason = xmpp_recode_in(text->value);
				error_destroy(channel, code, reason);
			}
		}
		break;
	}

out:
	g_free(reason);
}

void
muc_events_init(void)
{
	signal_add("xmpp recv message", sig_recv_message);
	signal_add("xmpp recv presence", sig_recv_presence);
	signal_add("xmpp recv iq", sig_recv_iq);
}

void
muc_events_deinit(void)
{
	signal_remove("xmpp recv message", sig_recv_message);
	signal_remove("xmpp recv presence", sig_recv_presence);
	signal_remove("xmpp recv iq", sig_recv_iq);
}
