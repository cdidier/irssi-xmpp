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
#include "ignore.h"
#include "levels.h"
#include "module-formats.h"
#include "nicklist.h"
#include "printtext.h"
#include "recode.h"
#include "settings.h"
#include "signals.h"
#include "window-items.h"
#include "fe-messages.h"
#include "fe-queries.h"
#include "fe-common/core/module-formats.h"
#include "fe-common/irc/module-formats.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-commands.h"
#include "xmpp-nicklist.h"
#include "xmpp-queries.h"
#include "xmpp-tools.h"

static void
sig_action(XMPP_SERVER_REC *server, const char *msg, const char *nick,
    const char *target, gpointer gpointer_type)
{
	void *item;
	char *freemsg;
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);

	type = GPOINTER_TO_INT(gpointer_type);

	level = MSGLEVEL_ACTIONS | (type == SEND_TARGET_CHANNEL ?
	    MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);

	if (type == SEND_TARGET_CHANNEL)
		item = xmpp_channel_find(server, target);
	else
		item = privmsg_get_query(SERVER(server), nick, FALSE, level);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);

	if (type == SEND_TARGET_CHANNEL) {
		if (item && window_item_is_active(item))
			printformat_module(IRC_MODULE_NAME, server, target,
			    level, IRCTXT_ACTION_PUBLIC, nick, msg);
		else
			printformat_module(IRC_MODULE_NAME, server, target,
			    level, IRCTXT_ACTION_PUBLIC_CHANNEL, nick,
			    target, msg);
	} else
		printformat_module(IRC_MODULE_NAME, server, nick, level,
		    (item == NULL) ?
		    IRCTXT_ACTION_PRIVATE : IRCTXT_ACTION_PRIVATE_QUERY,
		    nick, nick, msg);

	g_free(freemsg);
}

static void
sig_own_action(XMPP_SERVER_REC *server, const char *msg, const char *target,
    gpointer gpointer_type)
{
	void *item;
	char *freemsg = NULL;
	int type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(target != NULL);

	type = GPOINTER_TO_INT(gpointer_type);

	if (type == SEND_TARGET_CHANNEL)
		item = xmpp_channel_find(server, target);
	else
		item = xmpp_query_find(server, target);

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
sig_error(XMPP_SERVER_REC *server, const char *full_jid,
    const char *msg)
{
	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);

	printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
	    XMPPTXT_MESSAGE_NOT_DELIVERED, full_jid, msg);
}

static void
sig_channel_nick(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    NICK_REC *nick, const char *oldnick)
{
	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(oldnick != NULL);

	if (!IS_XMPP_SERVER(server) || !IS_XMPP_CHANNEL(channel)
	    || ignore_check(SERVER(server), oldnick, nick->host, channel->nick,
	    nick->nick, MSGLEVEL_NICKS))
		 return;

	printformat_module(CORE_MODULE_NAME, server, channel->name,
	    MSGLEVEL_NICKS, TXT_NICK_CHANGED, oldnick, nick->nick,
	    channel->name, nick->host);
}

static void
sig_channel_own_nick(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    NICK_REC *nick, const char *oldnick)
{
	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(oldnick != NULL);

	if (!IS_XMPP_SERVER(server) || !IS_XMPP_CHANNEL(channel)
	    || channel->ownnick != nick)
		return;

	printformat_module(CORE_MODULE_NAME, server, channel->name,
	    MSGLEVEL_NICKS | MSGLEVEL_NO_ACT, TXT_YOUR_NICK_CHANGED, oldnick,
	    nick->nick, channel->name, nick->host);
}

void
sig_nick_in_use(XMPP_CHANNEL_REC *channel, const char *nick)
{
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);

	if (!IS_XMPP_SERVER(channel->server) || !channel->joined)
		return;

	printformat_module(IRC_MODULE_NAME, channel->server, channel->name,
	    MSGLEVEL_CRAP, IRCTXT_NICK_IN_USE, nick);
}

static void
sig_channel_mode(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *nick, int affiliation, int role)
{
	char *mode;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);
	if (!IS_XMPP_SERVER(server) || !IS_XMPP_CHANNEL(channel))
		return;

	mode = g_strconcat("+", xmpp_nicklist_affiliation[affiliation], "/+",
	    xmpp_nicklist_role[role], " ", nick,  NULL);

	printformat_module(IRC_MODULE_NAME, server, channel->name, MSGLEVEL_MODES,
	    IRCTXT_CHANMODE_CHANGE, channel->name, mode, channel->name);

	g_free(mode);
}

static void
sig_message_own_public(XMPP_SERVER_REC *server, char *msg, char *target)
{
	WINDOW_REC *window;
	XMPP_CHANNEL_REC *channel;
	const char *nickmode;
	char *freemsg = NULL, *recoded;
	gboolean print_channel;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(target != NULL);

	if (!IS_XMPP_SERVER(server))
		return;

	channel = xmpp_channel_find(server, target);
	if (channel == NULL)
		return;

	nickmode = channel_get_nickmode(CHANNEL(channel), channel->nick);

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
		    TXT_OWN_MSG, channel->nick, recoded, nickmode);
	else
		printformat_module(CORE_MODULE_NAME, server, target,
		    MSGLEVEL_PUBLIC | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
		    TXT_OWN_MSG_CHANNEL, channel->nick, target, recoded,
		    nickmode);

	g_free(recoded);
	g_free_not_null(freemsg);

	signal_stop();
	/* emit signal for chat-completion */
}

static void
sig_message_own_private(XMPP_SERVER_REC *server, char *msg, char *target,
    char *origtarget)
{
	QUERY_REC *query;
	char *freemsg = NULL, *recoded;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);

	if (!IS_XMPP_SERVER(server))
		return;

	if (target == NULL) {
		/* this should only happen if some special target failed and
		 * we should display some error message. currently the special
		 * targets are only ',' and '.'. */
		g_return_if_fail(strcmp(origtarget, ",") == 0 ||
		    strcmp(origtarget, ".") == 0);

		printformat_module(CORE_MODULE_NAME, NULL, NULL,
		    MSGLEVEL_CLIENTNOTICE, *origtarget == ',' ?
		    TXT_NO_MSGS_GOT : TXT_NO_MSGS_SENT);
		signal_stop();
		return;
	}

	query = privmsg_get_query(SERVER(server), target, TRUE, MSGLEVEL_MSGS);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis((WI_ITEM_REC *) query, msg);

	/* ugly from irssi: recode the sent message back for printing */
        recoded = recode_in(SERVER(server), msg, target);

	printformat_module(CORE_MODULE_NAME, server, target,
	    MSGLEVEL_MSGS | MSGLEVEL_NOHILIGHT | MSGLEVEL_NO_ACT,
	    query == NULL ? TXT_OWN_MSG_PRIVATE : TXT_OWN_MSG_PRIVATE_QUERY,
	    target, msg, server->nickname);

	g_free(recoded);
	g_free_not_null(freemsg);

	signal_stop();
}

void
fe_xmpp_messages_init(void)
{
	signal_add("message xmpp action", (SIGNAL_FUNC)sig_action);
	signal_add("message xmpp own_action", (SIGNAL_FUNC)sig_own_action);
	signal_add("message xmpp error", (SIGNAL_FUNC)sig_error);
	signal_add("message xmpp channel nick", (SIGNAL_FUNC)sig_channel_nick);
	signal_add("message xmpp channel own_nick",
	    (SIGNAL_FUNC)sig_channel_own_nick);
	signal_add("xmpp channel nick in use", (SIGNAL_FUNC)sig_nick_in_use);
	signal_add("message xmpp channel mode", (SIGNAL_FUNC)sig_channel_mode);
	signal_add_first("message own_public",
	    (SIGNAL_FUNC)sig_message_own_public);
	signal_add_first("message own_private",
	    (SIGNAL_FUNC)sig_message_own_private);
}

void
fe_xmpp_messages_deinit(void)
{
	signal_remove("message xmpp action", (SIGNAL_FUNC)sig_action);
	signal_remove("message xmpp own_action", (SIGNAL_FUNC)sig_own_action);
	signal_remove("message xmpp error", (SIGNAL_FUNC)sig_error);
	signal_remove("message xmpp channel nick",
	    (SIGNAL_FUNC)sig_channel_nick);
	signal_remove("message xmpp channel own_nick",
	    (SIGNAL_FUNC)sig_channel_own_nick);
	signal_remove("xmpp channel nick in use",
	    (SIGNAL_FUNC)sig_nick_in_use);
	signal_remove("message xmpp channel mode", (SIGNAL_FUNC)sig_channel_mode);
	signal_remove("message own_public",
	    (SIGNAL_FUNC)sig_message_own_public);
	signal_remove("message own_private",
	    (SIGNAL_FUNC)sig_message_own_private);
}
