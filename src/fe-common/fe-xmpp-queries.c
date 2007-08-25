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
#include "module-formats.h"
#include "signals.h"
#include "window-items.h"
#include "levels.h"
#include "printtext.h"
#include "themes.h"

#include "xmpp-queries.h"
#include "xmpp-rosters.h"

const char *fe_xmpp_presence_show[] = {
	"error",
	"is now offline",
	"is now away: extended away",
	"is now away: do not disturb",
	"is now away",
	"is now online",
	"is now ready for chat"
};

static void
event_presence_change(XMPP_SERVER_REC *server, const char *full_jid,
    int show, const char *status)
{
	QUERY_REC *rec;
	const char *msg;

	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);
	g_return_if_fail(0 <= show && show < XMPP_PRESENCE_SHOW_LEN);

	rec = xmpp_query_find(server, full_jid);
	if (rec == NULL)
		return;

	msg = fe_xmpp_presence_show[show];

	if (status != NULL)
		printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
		    XMPPTXT_PRESENCE_CHANGE_REASON, full_jid, msg, status);
	else
		printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
		    XMPPTXT_PRESENCE_CHANGE, full_jid, msg);
}

static void
event_query_raise(XMPP_SERVER_REC *server, QUERY_REC *query)
{
	WINDOW_REC *window;
	
	g_return_if_fail(query != NULL);

	window = window_item_window(query);

	if (window != active_win)
		window_set_active(window);

	window_item_set_active(active_win, (WI_ITEM_REC *) query);
}

void
fe_xmpp_queries_init(void)
{
	signal_add("xmpp query raise", (SIGNAL_FUNC)event_query_raise);
	signal_add("xmpp jid presence change",
	    (SIGNAL_FUNC)event_presence_change);
}

void
fe_xmpp_queries_deinit(void)
{   
	signal_remove("xmpp query raise", (SIGNAL_FUNC)event_query_raise);
	signal_remove("xmpp jid presence change",
	    (SIGNAL_FUNC)event_presence_change);
}
