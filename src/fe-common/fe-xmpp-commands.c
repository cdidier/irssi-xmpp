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
#include "signals.h"
#include "window-items.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-commands.h"
#include "xmpp-queries.h"

static void
cmd_me(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	const char *target;
	char *text;
	int type;

	CMD_XMPP_SERVER(server);

	if (!IS_XMPP_ITEM(item) || !xmpp_server_is_alive(server))
		return;

	target = window_item_get_target(item);
	type = IS_CHANNEL(item) ? SEND_TARGET_CHANNEL : SEND_TARGET_NICK;

	signal_emit("message xmpp own_action", 3, server, data, target,
	    GINT_TO_POINTER(type));

	text = g_strconcat("/me ", data, NULL);
	server->send_message((SERVER_REC *)server, target, text, type);
	g_free(text);
}

void
fe_xmpp_commands_init(void)
{
	command_bind_xmpp("me", NULL, (SIGNAL_FUNC)cmd_me);
}

void
fe_xmpp_commands_deinit(void)
{
	command_unbind("me", (SIGNAL_FUNC)cmd_me);
}
