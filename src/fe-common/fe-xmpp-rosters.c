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
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "xmpp-rosters.h"
#include "fe-xmpp-status.h"

static void
sig_roster_group(XMPP_SERVER_REC *server, const char *group)
{
	WINDOW_REC *window = NULL;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	if (window != NULL)
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_ROSTER_GROUP, (group != NULL) ?
		    group : settings_get_str("roster_default_group"));
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_ROSTER_GROUP, (group != NULL) ?
		    group : settings_get_str("roster_default_group"));
}

static void
sig_roster_nick(XMPP_SERVER_REC *server, const XMPP_ROSTER_USER_REC *user)
{
	GSList *resource_list;
	XMPP_ROSTER_RESOURCE_REC *resource;
	WINDOW_REC *window;
	const char *first_show, *show;
	char *name, *resource_str, *str;

	window = NULL;
	first_show = NULL;
	resource_str = NULL;

	/* offline user ? */
	if (user->resources == NULL)
		first_show = xmpp_presence_show[XMPP_PRESENCE_UNAVAILABLE];

	resource_list = user->resources;
	while (resource_list != NULL) {
		resource = (XMPP_ROSTER_RESOURCE_REC *)resource_list->data;
		
		show = xmpp_presence_show[resource->show];
		
		if (first_show == NULL) 
			first_show = show;
		
		if(resource->show == XMPP_PRESENCE_AVAILABLE)
			show = NULL;
		
		if (resource->name != NULL) {
			str = g_strdup_printf("%s[%s%s%s%s(%d)%s%s]",
			    resource_str ? resource_str : "",
			    show ? "(" : "", show ? show : "", show ? ")" : "",
			    resource->name, resource->priority,
			    resource->status ? ": " : "",
			    resource->status ? resource->status : "");
			
			g_free(resource_str);
			resource_str = str;
		}
		
		resource_list = g_slist_next(resource_list);
	}

	if (user->error)
		first_show = xmpp_presence_show[XMPP_PRESENCE_ERROR];

	if (user->subscription != XMPP_SUBSCRIPTION_BOTH) {
		str = g_strdup_printf("%s%s(subscription: %s)",
		    resource_str ? resource_str : "",
		    resource_str ? " " : "",
		    xmpp_subscription[user->subscription]);
		
		g_free(resource_str);
		resource_str = str;
	}

	if (user->name != NULL) {
		name = user->name;

		str = g_strdup_printf("(%s) %s", user->jid,
			resource_str ? resource_str : "");
		g_free(resource_str);
		resource_str = str;

	} else
		name = user->jid;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	if (window != NULL)
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_ROSTER_NICK, first_show, name, resource_str);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_ROSTER_NICK, first_show, name, resource_str);
	
	g_free(resource_str);
}

static void
sig_begin_of_roster(XMPP_SERVER_REC *server)
{
	WINDOW_REC *window = NULL;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	if (window != NULL)
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_BEGIN_OF_ROSTER);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_BEGIN_OF_ROSTER);
}

static void
sig_end_of_roster(XMPP_SERVER_REC *server)
{
	WINDOW_REC *window = NULL;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	if (window != NULL)
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_END_OF_ROSTER);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_END_OF_ROSTER);
}

static void
sig_not_in_roster(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(jid != NULL);

	printformat_module(MODULE_NAME, server, NULL,
	    MSGLEVEL_CLIENTERROR, XMPPTXT_NOT_IN_ROSTER, jid);
}

static void
sig_subscribe(XMPP_SERVER_REC *server, const char *jid, const char *status)
{
	g_return_if_fail(jid != NULL);

	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBE, jid, status);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBE, jid, status);
}

static void
sig_subscribed(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(jid != NULL);

	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBED, jid);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBED, jid);
}

static void
sig_unsubscribe(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(jid != NULL);

	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBE, jid);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBE, jid);
}

static void
sig_unsubscribed(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(jid != NULL);

	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBED, jid);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBED, jid);
}

void
fe_xmpp_rosters_init(void)
{
	signal_add("xmpp roster group", (SIGNAL_FUNC)sig_roster_group);
	signal_add("xmpp roster nick", (SIGNAL_FUNC)sig_roster_nick);
	signal_add("xmpp begin of roster",(SIGNAL_FUNC)sig_begin_of_roster);
	signal_add("xmpp end of roster", (SIGNAL_FUNC)sig_end_of_roster);
	signal_add("xmpp not in roster", (SIGNAL_FUNC)sig_not_in_roster);
	signal_add("xmpp presence subscribe", (SIGNAL_FUNC)sig_subscribe);
	signal_add("xmpp presence subscribed", (SIGNAL_FUNC)sig_subscribed);
	signal_add("xmpp presence unsubscribe", (SIGNAL_FUNC)sig_unsubscribe);
	signal_add("xmpp presence unsubscribed",
	    (SIGNAL_FUNC)sig_unsubscribed);
}

void
fe_xmpp_rosters_deinit(void)
{
	signal_remove("xmpp roster group", (SIGNAL_FUNC)sig_roster_group);
	signal_remove("xmpp roster nick", (SIGNAL_FUNC)sig_roster_nick);
	signal_remove("xmpp begin of roster",
	    (SIGNAL_FUNC)sig_begin_of_roster);
	signal_remove("xmpp end of roster", (SIGNAL_FUNC)sig_end_of_roster);
	signal_remove("xmpp not in roster", (SIGNAL_FUNC) sig_not_in_roster);
	signal_remove("xmpp presence subscribe", (SIGNAL_FUNC)sig_subscribe);
	signal_remove("xmpp presence subscribed", (SIGNAL_FUNC)sig_subscribed);
	signal_remove("xmpp presence unsubscribe",
	    (SIGNAL_FUNC)sig_unsubscribe);
	signal_remove("xmpp presence unsubscribed",
	    (SIGNAL_FUNC)sig_unsubscribed);
}
