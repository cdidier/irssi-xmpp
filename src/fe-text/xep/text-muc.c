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

#include "module.h"
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-text/statusbar-item.h>
#include <irssi/src/fe-common/core/window-items.h>

#include "xmpp-servers.h"
#include "xep/muc.h"

static void
update_nick_statusbar(XMPP_SERVER_REC *server, MUC_REC *channel,
    gboolean redraw)
{
	char *newnick;

	newnick = (channel != NULL && IS_MUC(channel)) ? channel->nick
	    : (settings_get_bool("xmpp_set_nick_as_username") ?
	    server->user : server->jid);
	if (newnick == NULL)
		return;
	if (strcmp(server->nick, newnick) == 0)
		return;
	g_free(server->nick);
	server->nick = g_strdup(newnick);
	if (redraw)
		statusbar_items_redraw("user");
}

static void
sig_window_changed(WINDOW_REC *window, WINDOW_REC *oldwindow)
{
	XMPP_SERVER_REC *server;

	g_return_if_fail(window != NULL);
	if ((server = XMPP_SERVER(window->active_server)) == NULL)
		return;
	update_nick_statusbar(server, MUC(window->active), FALSE);
}

static void
sig_window_destroyed(WINDOW_REC *window)
{
	XMPP_SERVER_REC *server;
	MUC_REC *channel;

	g_return_if_fail(window != NULL);
	if ((server = XMPP_SERVER(window->active_server)) == NULL)
		return;
	channel = MUC(window->active);
	if (channel != NULL || !IS_MUC(active_win->active))
		update_nick_statusbar(server, NULL, TRUE);
}

static void
sig_nick_changed(MUC_REC *channel)
{
	g_return_if_fail(channel != NULL);

	if (!IS_MUC(channel))
		return;
	if (MUC(active_win->active) == channel)
		update_nick_statusbar(channel->server, channel, TRUE);
}

static void
sig_channel_joined(MUC_REC *channel)
{
	g_return_if_fail(channel != NULL);

	if (!IS_MUC(channel))
		return;
	if (MUC(active_win->active) == channel)
		update_nick_statusbar(channel->server, channel, TRUE);
}

static void
sig_channel_destroyed(MUC_REC *channel)
{
	g_return_if_fail(channel != NULL);

	if (!IS_MUC(channel))
		return;
	if (MUC(active_win->active) == channel)
		update_nick_statusbar(channel->server, NULL, TRUE);
}

void
text_muc_init(void)
{
	signal_add("window changed", sig_window_changed);
	signal_add("window destroyed", sig_window_destroyed);
	signal_add("message xmpp channel own_nick", sig_nick_changed);
	signal_add("channel joined", sig_channel_joined);
	signal_add("channel destroyed", sig_channel_destroyed);
}

void
text_muc_deinit(void)
{
	signal_remove("window changed", sig_window_changed);
	signal_remove("window destroyed", sig_window_destroyed);
	signal_remove("message xmpp channel own_nick", sig_nick_changed);
	signal_remove("channel joined", sig_channel_joined);
	signal_remove("channel destroyed", sig_channel_destroyed);
}
