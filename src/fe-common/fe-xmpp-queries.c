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
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "signals.h"
#include "window-items.h"

#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "fe-xmpp-status.h"

static void
sig_presence_changed(XMPP_SERVER_REC *server, const char *full_jid,
    int show, const char *status)
{
	XMPP_QUERY_REC *rec;
	XMPP_ROSTER_USER_REC *user;
	const char *msg;
	char *name;

	g_return_if_fail(server != NULL);
	g_return_if_fail(full_jid != NULL);
	g_return_if_fail(0 <= show && show < XMPP_PRESENCE_SHOW_LEN);

	rec = xmpp_query_find(server, full_jid);
	if (rec == NULL)
		return;

	msg = fe_xmpp_presence_show[show];

	user = xmpp_rosters_find_user(server->roster, full_jid, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
		XMPPTXT_FORMAT_NAME, user->name, full_jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
		XMPPTXT_FORMAT_JID, full_jid);

	if (status != NULL)
		printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
		    XMPPTXT_PRESENCE_CHANGE_REASON, name, msg, status);
	else
		printformat_module(MODULE_NAME, server, full_jid, MSGLEVEL_CRAP,
		    XMPPTXT_PRESENCE_CHANGE, name, msg);
}

static void
sig_query_raise(XMPP_SERVER_REC *server, QUERY_REC *query)
{
	WINDOW_REC *window;
	
	g_return_if_fail(query != NULL);

	window = window_item_window(query);

	if (window != active_win)
		window_set_active(window);

	window_item_set_active(active_win, (WI_ITEM_REC *)query);
}

static void
sig_query_created(XMPP_QUERY_REC *query, int automatic)
{
	XMPP_ROSTER_USER_REC *user;

	if (!IS_XMPP_QUERY(query))
		return;

	user = xmpp_rosters_find_user(query->server->roster, query->name, NULL);
	if (user == NULL || user->name == NULL)
		return;

	printformat_module(MODULE_NAME, query->server, query->name,
	    MSGLEVEL_CRAP, XMPPTXT_QUERY_AKA, user->jid, user->name);
}

void
fe_xmpp_queries_init(void)
{
	signal_add("xmpp query raise", (SIGNAL_FUNC)sig_query_raise);
	signal_add("xmpp presence changed", (SIGNAL_FUNC)sig_presence_changed);
	signal_add_last("query created", (SIGNAL_FUNC)sig_query_created);
}

void
fe_xmpp_queries_deinit(void)
{   
	signal_remove("xmpp query raise", (SIGNAL_FUNC)sig_query_raise);
	signal_remove("xmpp presence changed",
	    (SIGNAL_FUNC)sig_presence_changed);
	signal_remove("query created", (SIGNAL_FUNC)sig_query_created);
}
