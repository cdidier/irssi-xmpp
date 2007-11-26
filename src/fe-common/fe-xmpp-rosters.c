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

#include <string.h>

#include "module.h"
#include "formats.h"
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

	printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
	    XMPPTXT_ROSTER_GROUP, (group != NULL) ?
	    group : settings_get_str("roster_default_group"));
}

static const char *
get_first_show(GSList *list)
{
	if (list == NULL)
		return NULL;
	return xmpp_presence_show[
	    ((XMPP_ROSTER_RESOURCE_REC *)list->data)->show];
}

static char *
get_resources(XMPP_SERVER_REC *server, GSList *list)
{
	GSList *tmp;
	GString *resources;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *show, *status, *priority, *text;

	if (list == NULL)
		return NULL;

	resources = g_string_new(NULL);

	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		resource = tmp->data;

		show = (resource->show == XMPP_PRESENCE_AVAILABLE) ? NULL :
		    format_get_text(MODULE_NAME, NULL, server, NULL,
		    XMPPTXT_ROSTER_RESOURCE_SHOW,
		    xmpp_presence_show[resource->show]);

		status = (resource->status == NULL) ? NULL :
		    format_get_text(MODULE_NAME, NULL, server, NULL,
		    XMPPTXT_ROSTER_RESOURCE_STATUS, resource->status);

		priority = g_strdup_printf("%d", resource->priority);

		text = format_get_text(MODULE_NAME, NULL, server, NULL,
		    XMPPTXT_ROSTER_RESOURCE, show, resource->name, priority,
		    status);

		g_free(show);
		g_free(status);
		g_free(priority);

		g_string_append(resources, text);

		g_free(text);
	}

	text = resources->str;
	g_string_free(resources, FALSE);

	return text;
}

static void
sig_roster_nick(XMPP_SERVER_REC *server, const XMPP_ROSTER_USER_REC *user)
{
	WINDOW_REC *window;
	const char *first_show;
	char *resources, *subscription;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	if (user->error)
		first_show = xmpp_presence_show[XMPP_PRESENCE_ERROR];
	else if (user->resources == NULL)
		first_show = xmpp_presence_show[XMPP_PRESENCE_UNAVAILABLE];
	else
		first_show = get_first_show(user->resources);

	resources = get_resources(server, user->resources);

	subscription = (user->subscription == XMPP_SUBSCRIPTION_BOTH) ? NULL :
	    format_get_text(MODULE_NAME, window, server, NULL,
	    XMPPTXT_ROSTER_SUBSCRIPTION,
	    xmpp_subscription[user->subscription]);

	if (user->name != NULL)	
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
	    	    XMPPTXT_ROSTER_NAME, first_show, user->name, user->jid,
		    resources, subscription);
	else
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
	    	    XMPPTXT_ROSTER_JID, first_show, user->jid,
		    resources, subscription);
	
	g_free(resources);
	g_free(subscription);
}

static void
sig_begin_of_roster(XMPP_SERVER_REC *server)
{
	WINDOW_REC *window = NULL;
	char *show, *status, *priority, *text, *resources;

	if (settings_get_bool("xmpp_status_window")) { 
		window = fe_xmpp_status_get_window(server);
		if (window != active_win)
			window = NULL;
	}

	show = (server->show == XMPP_PRESENCE_AVAILABLE) ? NULL :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	    XMPPTXT_ROSTER_RESOURCE_SHOW,
	    xmpp_presence_show[server->show]);

	status = (server->away_reason == NULL
	    || strcmp(server->away_reason, " ") == 0) ? NULL :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	    XMPPTXT_ROSTER_RESOURCE_STATUS, server->away_reason);

	priority = g_strdup_printf("%d", server->priority);

	text = format_get_text(MODULE_NAME, NULL, server, NULL,
	    XMPPTXT_ROSTER_RESOURCE, show,  server->resource, priority,
	    status);

	g_free(show);
	g_free(status);
	g_free(priority);

	resources = get_resources(server, server->resources);

	printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
	    XMPPTXT_BEGIN_OF_ROSTER, server->jid, text, resources);

	g_free(text);
	g_free(resources);
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

	printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
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
