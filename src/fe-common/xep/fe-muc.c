/*
 * $Id$
 *
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

#include "module.h"
#include "ignore.h"
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "signals.h"
#include "fe-common/core/module-formats.h"
#include "fe-common/irc/module-formats.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "xep/muc.h"
#include "xep/muc-nicklist.h"

static void
sig_invite(XMPP_SERVER_REC *server, const char *from, const char *channame)
{
	const char *name;

	name = rosters_get_name(server, from);
	if (name == NULL)
		name = from;
	printformat_module(CORE_MODULE_NAME, server, from, MSGLEVEL_INVITES,
	    TXT_INVITE, name, channame, from);
}

static void
sig_joinerror(MUC_REC *channel, gpointer error)
{
	char *reason;

	g_return_if_fail(IS_MUC(channel));
	switch(GPOINTER_TO_INT(error)) {
	case MUC_ERROR_PASSWORD_INVALID_OR_MISSING:
		reason = "Password required";
		break;
	case MUC_ERROR_USER_BANNED:
		reason = "Banned from the room";
		break;
	case MUC_ERROR_ROOM_NOT_FOUND:
		reason = "The room does not exist";
		break;
	case MUC_ERROR_ROOM_CREATION_RESTRICTED:
		reason = "Room creation is restricted";
		break;
	case MUC_ERROR_USE_RESERVED_ROOM_NICK:
		reason = "Your desired nick is reserved (Retrying with your alternate nick...)";
		break;
	case MUC_ERROR_NOT_ON_MEMBERS_LIST:
		reason = "You are not on the member list";
		break;
	case MUC_ERROR_NICK_IN_USE:
		reason = "Your desired nick is already in use (Retrying with your alternate nick...)";
		break;
	case MUC_ERROR_MAXIMUM_USERS_REACHED:
		reason = "Maximum number of users has been reached";
	default:
		reason = "Unknow reason";
	}
	printformat_module(MODULE_NAME, channel->server, NULL,
	    MSGLEVEL_CRAP, XMPPTXT_CHANNEL_JOINERROR,
	    channel->name, reason);
}

static void
sig_nick(MUC_REC *channel, NICK_REC *nick, const char *oldnick)
{
	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(nick != NULL);
	g_return_if_fail(oldnick != NULL);
	if (ignore_check(SERVER(channel->server), oldnick, nick->host,
	    channel->nick, nick->nick, MSGLEVEL_NICKS))
		 return;
	printformat_module(CORE_MODULE_NAME, channel->server, channel->name,
	    MSGLEVEL_NICKS, TXT_NICK_CHANGED, oldnick, nick->nick,
	    channel->name, nick->host);
}

static void
sig_own_nick(MUC_REC *channel, NICK_REC *nick, const char *oldnick)
{
	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(nick != NULL);
	g_return_if_fail(oldnick != NULL);

	if (channel->ownnick != nick)
		return;
	printformat_module(CORE_MODULE_NAME, channel->server, channel->name,
	    MSGLEVEL_NICKS | MSGLEVEL_NO_ACT, TXT_YOUR_NICK_CHANGED, oldnick,
	    nick->nick, channel->name, nick->host);
}

void
sig_nick_in_use(MUC_REC *channel, const char *nick)
{
	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(nick != NULL);
	if (!channel->joined)
		return;
	printformat_module(IRC_MODULE_NAME, channel->server, channel->name,
	    MSGLEVEL_CRAP, IRCTXT_NICK_IN_USE, nick);
}

static void
sig_mode(MUC_REC *channel, const char *nick, int affiliation,
    int role)
{
	char *mode, *affiliation_str, *role_str;

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(nick != NULL);
	switch (affiliation) {
	case XMPP_NICKLIST_AFFILIATION_OWNER:
		affiliation_str = "O";
		break;
	case XMPP_NICKLIST_AFFILIATION_ADMIN:
		affiliation_str = "A";
		break;
	case XMPP_NICKLIST_AFFILIATION_MEMBER:
		affiliation_str = "M";
		break;
	case XMPP_NICKLIST_AFFILIATION_OUTCAST:
		affiliation_str = "U";
		break;
	default:
		affiliation_str = "";
	}
	switch (role) {
	case XMPP_NICKLIST_ROLE_MODERATOR:
		role_str = "m";
		break;
	case XMPP_NICKLIST_ROLE_PARTICIPANT:
		role_str = "p";
		break;
	case XMPP_NICKLIST_ROLE_VISITOR:
		role_str = "v";
		break;
	default:
		role_str = "";
	}
	if (*affiliation_str == '\0' && *role_str == '\0')
		return;
	mode = g_strconcat("+", affiliation_str, role_str, " ", nick,  NULL);
	printformat_module(IRC_MODULE_NAME, channel->server, channel->name,
	    MSGLEVEL_MODES, IRCTXT_CHANMODE_CHANGE, channel->name, mode,
	    channel->name);
	g_free(mode);
}

void
fe_muc_init(void)
{
	signal_add("xmpp invite", sig_invite);
	signal_add("xmpp muc joinerror", sig_joinerror);
	signal_add("message xmpp muc nick", sig_nick);
	signal_add("message xmpp muc own_nick", sig_own_nick);
	signal_add("message xmpp muc nick in use", sig_nick_in_use);
	signal_add("message xmpp muc mode", sig_mode);
}

void
fe_muc_deinit(void)
{
	signal_remove("xmpp invite", sig_invite);
	signal_remove("xmpp muc joinerror", sig_joinerror);
	signal_remove("message xmpp muc nick", sig_nick);
	signal_remove("message xmpp muc own_nick", sig_own_nick);
	signal_remove("message xmpp muc nick in use", sig_nick_in_use);
	signal_remove("message xmpp muc mode", sig_mode);
}

