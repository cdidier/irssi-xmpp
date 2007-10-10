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
update_nick_statusbar(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel,
    gboolean redraw)
{
	g_return_if_fail(server != NULL);

	if (!IS_XMPP_SERVER(server))
		return;

	g_free(server->nick);
	server->nick = g_strdup(IS_XMPP_CHANNEL(channel) ?
	    channel->nick : server->nickname);

	if (redraw)
		statusbar_redraw(NULL, TRUE);
}

static void
sig_window_changed(WINDOW_REC *window, WINDOW_REC *oldwindow)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(window != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	channel = XMPP_CHANNEL(window->active);
	if (channel != NULL ||
	    (oldwindow != NULL && IS_XMPP_CHANNEL(oldwindow->active)))
		update_nick_statusbar(server, channel, FALSE);
}

static void
sig_window_destroyed(WINDOW_REC *window)
{
	XMPP_SERVER_REC *server;
	XMPP_CHANNEL_REC *channel;

	g_return_if_fail(window != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	channel = XMPP_CHANNEL(window->active);
	if (channel != NULL || !IS_XMPP_CHANNEL(active_win->active))
		update_nick_statusbar(server, NULL, TRUE);
}

static void
sig_nick_changed(XMPP_SERVER_REC *server, XMPP_CHANNEL_REC *channel)
{
	if (!IS_XMPP_SERVER(server) || !IS_XMPP_CHANNEL(channel))
		return;

	if (XMPP_CHANNEL(active_win->active) == channel)
		update_nick_statusbar(server, channel, TRUE);
}

static void
sig_channel_joined(XMPP_CHANNEL_REC *channel)
{
	g_return_if_fail(channel != NULL);

	if (!IS_XMPP_CHANNEL(channel))
		return;

	if (XMPP_CHANNEL(active_win->active) == channel)
		update_nick_statusbar(channel->server, channel, TRUE);
}

static void
sig_channel_destroyed(XMPP_CHANNEL_REC *channel)
{
	g_return_if_fail(channel != NULL);

	if (!IS_XMPP_CHANNEL(channel))
		return;

	if (XMPP_CHANNEL(active_win->active) == channel)
		update_nick_statusbar(channel->server, NULL, TRUE);
}

void
text_xmpp_nick_init(void)
{
	signal_add("window changed", (SIGNAL_FUNC)sig_window_changed);
	signal_add("window destroyed", (SIGNAL_FUNC)sig_window_destroyed);
	signal_add("message xmpp channel own_nick",
	    (SIGNAL_FUNC)sig_nick_changed);
	signal_add("channel joined",(SIGNAL_FUNC)sig_channel_joined);
	signal_add("channel destroyed",(SIGNAL_FUNC)sig_channel_destroyed);
}

void
text_xmpp_nick_deinit(void)
{
	signal_remove("window changed", (SIGNAL_FUNC)sig_window_changed);
	signal_remove("window destroyed", (SIGNAL_FUNC)sig_window_destroyed);
	signal_remove("message xmpp channel own_nick",
	    (SIGNAL_FUNC)sig_nick_changed);
	signal_remove("channel joined",(SIGNAL_FUNC)sig_channel_joined);
	signal_remove("channel destroyed",(SIGNAL_FUNC)sig_channel_destroyed);
}
