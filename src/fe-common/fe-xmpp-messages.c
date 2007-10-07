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
#include "printtext.h"
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
#include "xmpp-queries.h"

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
    const char *newnick, const char *oldnick)
{
	char *addr;
	int level;
	gboolean ownnick;

	addr = g_strconcat(channel->name, "/", newnick, NULL);

	if (ignore_check(SERVER(server), oldnick, addr, channel->nick, newnick,
	    MSGLEVEL_NICKS))
		 goto out;

	ownnick = (strcmp(newnick, channel->nick) == 0);

	level = MSGLEVEL_NICKS;
	if (ownnick)
		level |= MSGLEVEL_NO_ACT;

	printformat_module(CORE_MODULE_NAME, server, channel->name, level,
	    ownnick ? TXT_YOUR_NICK_CHANGED : TXT_NICK_CHANGED,
	    oldnick, newnick, channel, addr);

out:
	g_free(addr);
}

static void
sig_channel_mode(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    const char *nick, const char *affiliation, const char *role)
{
	char *mode;

	g_return_if_fail(server != NULL);
	g_return_if_fail(channel != NULL);
	g_return_if_fail(nick != NULL);

	mode = g_strconcat("+", affiliation, "/+", role, " ", nick,  NULL);

	printformat_module(IRC_MODULE_NAME, server, channel->name, MSGLEVEL_MODES,
	    IRCTXT_CHANMODE_CHANGE, channel->name, mode, channel->name);

	g_free(mode);
}

void
fe_xmpp_messages_init(void)
{
	signal_add("message xmpp action", (SIGNAL_FUNC)sig_action);
	signal_add("message xmpp own_action", (SIGNAL_FUNC)sig_own_action);
	signal_add("message xmpp error", (SIGNAL_FUNC)sig_error);
	signal_add("message xmpp channel nick", (SIGNAL_FUNC)sig_channel_nick);
	signal_add("message xmpp channel mode", (SIGNAL_FUNC)sig_channel_mode);
}

void
fe_xmpp_messages_deinit(void)
{
	signal_remove("message xmpp action", (SIGNAL_FUNC)sig_action);
	signal_remove("message xmpp own_action", (SIGNAL_FUNC)sig_own_action);
	signal_remove("message xmpp error", (SIGNAL_FUNC)sig_error);
	signal_remove("message xmpp channel nick",
	    (SIGNAL_FUNC)sig_channel_nick);
	signal_remove("message xmpp channel mode", (SIGNAL_FUNC)sig_channel_mode);
}
