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

#include "module.h"
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "signals.h"
#include "fe-common/core/module-formats.h"
#include "fe-common/irc/module-formats.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"

static void
sig_joinerror(XMPP_CHANNEL_REC *channel, int error)
{
	char *reason;

	if (!IS_XMPP_CHANNEL(channel))
		return;

	switch(error) {
	case XMPP_CHANNELS_ERROR_PASSWORD_INVALID_OR_MISSING:
		reason = "Password required";
		break;
	case XMPP_CHANNELS_ERROR_USER_BANNED:
		reason = "Banned from the room";
		break;
	case XMPP_CHANNELS_ERROR_ROOM_NOT_FOUND:
		reason = "The room does not exist";
		break;
	case XMPP_CHANNELS_ERROR_ROOM_CREATION_RESTRICTED:
		reason = "Room creation is restricted";
		break;
	case XMPP_CHANNELS_ERROR_USE_RESERVED_ROOM_NICK:
		reason = "Your desired nick is reserved (Retrying with your alternate nick...)";
		break;
	case XMPP_CHANNELS_ERROR_NOT_ON_MEMBERS_LIST:
		reason = "You are not on the member list";
		break;
	case XMPP_CHANNELS_ERROR_NICK_IN_USE:
		reason = "Your desired nick is already in use (Retrying with your alternate nick...)";
		break;
	case XMPP_CHANNELS_ERROR_MAXIMUM_USERS_REACHED:
		reason = "Maximum number of users has been reached";
	default:
		reason = "Unknow reason";
	}

	printformat_module(MODULE_NAME, channel->server, NULL,
	    MSGLEVEL_CRAP, XMPPTXT_CHANNEL_JOINERROR,
	    channel->name, reason);
}

void
fe_xmpp_channels_init(void)
{
	signal_add("xmpp channel joinerror", (SIGNAL_FUNC)sig_joinerror);
}

void
fe_xmpp_channels_deinit(void)
{
	signal_remove("xmpp channel joinerror", (SIGNAL_FUNC)sig_joinerror);
}
