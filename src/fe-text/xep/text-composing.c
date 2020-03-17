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
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-text/statusbar-item.h>
#include <irssi/src/fe-common/core/window-items.h>

#include "xmpp-servers.h"
#include "xmpp-queries.h"

static void
item_xmpp_composing(struct SBAR_ITEM_REC *item, int get_size_only)
{
	XMPP_SERVER_REC *server;
	XMPP_QUERY_REC *query;
	char *str = NULL;

	server = XMPP_SERVER(active_win->active_server);
	if (server == NULL || !IS_XMPP_SERVER(server))
		goto out;
	query = XMPP_QUERY(active_win->active);
	if (query == NULL)
		goto out;
	if (query->composing_visible)
		str = "{sb composing}";

out:
	if (str == NULL) {
		if (get_size_only)
			statusbar_item_set_size(item, 0, 0);
		return;
	}
	statusbar_item_default_handler(item, get_size_only,
	    str, "", FALSE);
}

static void
xmpp_composing_update(void)
{
	statusbar_items_redraw("xmpp_composing");
}

static void
event_message_sent(XMPP_SERVER_REC *server, const char *message,
    const char *full_jid, const char *ignore)
{
	XMPP_QUERY_REC *query;

	if (!IS_XMPP_SERVER(server))
		return;
	query = xmpp_query_find(server, full_jid);
	if (query != NULL)
		query->composing_visible = FALSE;
	xmpp_composing_update();
}

void
text_composing_init(void)
{
	statusbar_item_register("xmpp_composing", NULL, item_xmpp_composing);

	signal_add("window changed", xmpp_composing_update);
	signal_add_last("xmpp composing show", xmpp_composing_update);
	signal_add_last("xmpp composing hide", xmpp_composing_update);
	signal_add("message private", event_message_sent);
	signal_add("message xmpp action", event_message_sent);
}

void
text_composing_deinit(void)
{
	statusbar_item_unregister("xmpp_composing");

	signal_remove("window changed", xmpp_composing_update);
	signal_remove("xmpp composing show", xmpp_composing_update);
	signal_remove("xmpp composing hide", xmpp_composing_update);
	signal_remove("message private", event_message_sent);
	signal_remove("message xmpp action", event_message_sent);
}
