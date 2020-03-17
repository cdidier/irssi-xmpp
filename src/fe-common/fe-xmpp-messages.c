/*
 * Copyright (C) 2007 Colin DIDIER
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

#include <string.h>

#include "module.h"
#include <irssi/src/core/channels.h>
#include <irssi/src/core/levels.h>
#include "module-formats.h"
#include <irssi/src/core/nicklist.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/recode.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/fe-common/core/fe-queries.h>
#include <irssi/src/fe-common/core/module-formats.h>
#include <irssi/src/fe-common/core/fe-messages.h>
#include <irssi/src/fe-common/irc/module-formats.h>
#include <irssi/irssi-version.h>

#include "xmpp-servers.h"

static void
sig_history(SERVER_REC *server, const char *msg, const char *nick,
    const char *target, const char *stamp, gpointer gpointer_type)
{
	void *item;
	char *text, *freemsg = NULL;
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);
	type = GPOINTER_TO_INT(gpointer_type);
	level = MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT
	    | (type == SEND_TARGET_CHANNEL ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)channel_find(server, target) : query_find(server, nick);
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);
	/* MUC */
	if (type == SEND_TARGET_CHANNEL) {
		CHANNEL_REC *chanrec = item;
		int print_channel;
		char *nickmode;

		print_channel = chanrec == NULL ||
		    !window_item_is_active((WI_ITEM_REC *)chanrec);
		if (!print_channel
		    && settings_get_bool("print_active_channel")
		    && window_item_window((WI_ITEM_REC *)chanrec)->items->next
		    != NULL)
			print_channel = TRUE;
		nickmode = channel_get_nickmode(chanrec, nick);
		text = !print_channel ?
		    format_get_text(CORE_MODULE_NAME, NULL, server,
			target, TXT_PUBMSG, nick, msg, nickmode) :
		    format_get_text(CORE_MODULE_NAME, NULL, server,
			target, TXT_PUBMSG_CHANNEL, nick, target, msg,
			nickmode);
		g_free(nickmode);
	/* General */
	} else
		text = format_get_text(CORE_MODULE_NAME, NULL, server,
		    target, item == NULL ? TXT_MSG_PRIVATE :
		    TXT_MSG_PRIVATE_QUERY, nick, nick, msg);
	printformat_module(MODULE_NAME, server, target,
	    level, XMPPTXT_MESSAGE_TIMESTAMP,
	    stamp, text);
	g_free_not_null(freemsg);
	g_free(text);
}

static void
sig_history_action(SERVER_REC *server, const char *msg, const char *nick,
    const char *target, const char *stamp, gpointer gpointer_type)
{
	void *item;
	char *text, *freemsg = NULL;
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);
	type = GPOINTER_TO_INT(gpointer_type);
	level = MSGLEVEL_ACTIONS | MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT
	    | (type == SEND_TARGET_CHANNEL ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)channel_find(server, target) : query_find(server, nick);
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);
	/* MUC */
	if (type == SEND_TARGET_CHANNEL) {
		if (item && window_item_is_active(item))
			text = format_get_text(IRC_MODULE_NAME, NULL, server,
			    target, IRCTXT_ACTION_PUBLIC, nick, msg);
		else
			text = format_get_text(IRC_MODULE_NAME, NULL, server,
			    target, IRCTXT_ACTION_PUBLIC_CHANNEL, nick,
			    target, msg);
	/* General */
	} else
		text = format_get_text(IRC_MODULE_NAME, NULL, server,
		    nick, (item == NULL) ? IRCTXT_ACTION_PRIVATE : 
		    IRCTXT_ACTION_PRIVATE_QUERY, nick, nick, msg);
	printformat_module(MODULE_NAME, server, target, level,
	    XMPPTXT_MESSAGE_TIMESTAMP, stamp, text);
	g_free(freemsg);
}

static void
sig_action(SERVER_REC *server, const char *msg, const char *nick,
    const char *target, gpointer gpointer_type)
{
	void *item;
	char *freemsg = NULL;
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);
	type = GPOINTER_TO_INT(gpointer_type);
	level = MSGLEVEL_ACTIONS | (type == SEND_TARGET_CHANNEL ?
	    MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)channel_find(server, target)
	    : privmsg_get_query(SERVER(server), nick, FALSE, level);
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);
	/* MUC */
	if (type == SEND_TARGET_CHANNEL) {
		if (item && window_item_is_active(item))
			printformat_module(IRC_MODULE_NAME, server, target,
			    level, IRCTXT_ACTION_PUBLIC, nick, msg);
		else
			printformat_module(IRC_MODULE_NAME, server, target,
			    level, IRCTXT_ACTION_PUBLIC_CHANNEL, nick,
			    target, msg);
	/* General */
	} else
		printformat_module(IRC_MODULE_NAME, server, nick, level,
		    (item == NULL) ?
		    IRCTXT_ACTION_PRIVATE : IRCTXT_ACTION_PRIVATE_QUERY,
		    nick, nick, msg);
	g_free(freemsg);
}

static void
sig_own_action(SERVER_REC *server, const char *msg, const char *target,
    gpointer gpointer_type)
{
	void *item;
	char *freemsg = NULL;
	int type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(target != NULL);

	type = GPOINTER_TO_INT(gpointer_type);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)channel_find(server, target) : query_find(server, target);
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);
	printformat_module(IRC_MODULE_NAME, server, target,
	    MSGLEVEL_ACTIONS | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT |
	    ((type == SEND_TARGET_CHANNEL) ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS),
	    (item != NULL) ? IRCTXT_OWN_ACTION : IRCTXT_OWN_ACTION_TARGET,
	    server->nick, msg, target);
	g_free(freemsg);
}

static void
sig_room(SERVER_REC *server, const char *msg, const char *target)
{
	CHANNEL_REC *channel;
	char *freemsg = NULL;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(target != NULL);

	channel = channel_find(server, target);
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *)channel, msg);
	printformat_module(MODULE_NAME, server, target, MSGLEVEL_PUBLIC,
	    XMPPTXT_MESSAGE_ROOM, msg);
	g_free(freemsg);
}

static void
sig_error(XMPP_SERVER_REC *server, const char *full_jid,
    const char *msg)
{
	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);

	printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
	    XMPPTXT_MESSAGE_NOT_DELIVERED, full_jid, msg);
}

static void
sig_message_own_public(SERVER_REC *server, char *msg, char *target)
{
	WINDOW_REC *window;
	CHANNEL_REC *channel;
	char *nick, *nickmode, *freemsg = NULL, *recoded;
	gboolean print_channel;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(target != NULL);
	if (!IS_XMPP_SERVER(server))
		return;
	channel = channel_find(server, target);
	if (channel == NULL || channel->ownnick == NULL)
		return;
	nick = channel->ownnick->nick;
	nickmode = channel_get_nickmode(CHANNEL(channel), nick);
	window = (channel == NULL) ?
	    NULL : window_item_window((WI_ITEM_REC *)channel);
	print_channel = (window == NULL ||
	    window->active != (WI_ITEM_REC *) channel);
	if (!print_channel && settings_get_bool("print_active_channel") &&
	    window != NULL && g_slist_length(window->items) > 1)
		print_channel = TRUE;
	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *)channel, msg);
	/* ugly from irssi: recode the sent message back for printing */
	recoded = recode_in(SERVER(server), msg, target);
	if (!print_channel)
		printformat_module(CORE_MODULE_NAME, server, target,
		    MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    TXT_OWN_MSG, nick, recoded, nickmode);
	else
		printformat_module(CORE_MODULE_NAME, server, target,
		    MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    TXT_OWN_MSG_CHANNEL, nick, target, recoded, nickmode);
	g_free(recoded);
	g_free(nickmode);
	g_free_not_null(freemsg);
	signal_stop();
	/* emit signal for chat-completion */
}

static void
sig_message_ignore(XMPP_SERVER_REC *server)
{
	if (IS_XMPP_SERVER(server))
		signal_stop();
}

void
fe_xmpp_messages_init(void)
{
	signal_add("message xmpp history", sig_history);
	signal_add("message xmpp history action", sig_history_action);
	signal_add("message xmpp action", sig_action);
	signal_add("message xmpp own_action", sig_own_action);
	signal_add("message xmpp room", sig_room);
	signal_add("message xmpp error", sig_error);
	signal_add_first("message xmpp own_public", sig_message_own_public);
	signal_add_first("message own_public", sig_message_ignore);
}

void
fe_xmpp_messages_deinit(void)
{
	signal_remove("message xmpp history", sig_history);
	signal_remove("message xmpp history action", sig_history_action);
	signal_remove("message xmpp action", sig_action);
	signal_remove("message xmpp own_action", sig_own_action);
	signal_remove("message xmpp room", sig_room);
	signal_remove("message xmpp error", sig_error);
	signal_remove("message xmpp own_public", sig_message_own_public);
	signal_remove("message own_public", sig_message_ignore);
}
