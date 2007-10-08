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

/* in include/irssi/src/fe-text */
#include "statusbar.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"

static void
sig_nick_changed(XMPP_SERVER_REC *server, const char *channame)
{
	XMPP_CHANNEL_REC *channel;

	if (!IS_XMPP_SERVER(server))
		return;

	channel = xmpp_channel_find(server, channame);
	if (channel != NULL && XMPP_CHANNEL(active_win->active) == channel) {
		g_free(server->nick);
		server->nick = g_strdup(channel->nick);

		statusbar_redraw(NULL, FALSE);
	}
}

static void
sig_window_changed(WINDOW_REC *window, WINDOW_REC *oldwindow)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	channel = XMPP_CHANNEL(window->active);
	if (channel != NULL || IS_XMPP_CHANNEL(oldwindow->active)) {
		g_free(server->nick);
		server->nick = g_strdup((channel != NULL) ?
		    channel->nick : server->orignick);
	}
}

void
text_xmpp_nick_init(void)
{
	signal_add("window changed", (SIGNAL_FUNC)sig_window_changed);
	signal_add_last("xmpp channels own_nick changed",
	    (SIGNAL_FUNC)sig_nick_changed);
}

void
text_xmpp_nick_deinit(void)
{
	signal_remove("window changed", (SIGNAL_FUNC)sig_window_changed);
	signal_remove("xmpp channels own_nick changed",
	    (SIGNAL_FUNC)sig_nick_changed);
}
