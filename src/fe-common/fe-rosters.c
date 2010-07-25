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

#include <string.h>

#include "module.h"
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "fe-xmpp-status.h"

static void
stroneline(char *s)
{
	size_t len, i, spaces;
	char *p1, *p2;
              
	for (p1 = s; *p1 != '\0'; ++p1) {
		if (isspace(*p1)) {
			*p1 = ' ';
			p2 = p1;
			spaces = 0;
			while (*(p2++) != '\0' && isspace(*p2))
			++spaces;
			if (spaces > 0) {
				len = strlen(p1);
				for (i = 0; i < len-spaces+1; ++i)
					p1[i-spaces] = p1[i];
			}
		}
	}
}

static gboolean
user_is_shown(XMPP_ROSTER_USER_REC *user)
{
	g_return_val_if_fail(user != NULL, FALSE);
	return user->resources != NULL
	    || (user->subscription == XMPP_SUBSCRIPTION_BOTH
	    && settings_get_bool("xmpp_roster_show_offline"))
	    || (user->subscription != XMPP_SUBSCRIPTION_BOTH
	    && (settings_get_bool("xmpp_roster_show_unsubscribed")
	    || settings_get_bool("xmpp_roster_show_offline")));
}

static void
show_group(XMPP_SERVER_REC *server, XMPP_ROSTER_GROUP_REC *group)
{
	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(group != NULL);
	printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
	    XMPPTXT_ROSTER_GROUP, (group->name != NULL) ?
	    group->name : settings_get_str("xmpp_roster_default_group"));
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
	char *show, *status, *status_str, *priority, *text;

	if (list == NULL)
		return NULL;
	resources = g_string_new(NULL);
	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		resource = tmp->data;
		show = (resource->show == XMPP_PRESENCE_AVAILABLE) ? NULL :
		    format_get_text(MODULE_NAME, NULL, server, NULL,
		        XMPPTXT_FORMAT_RESOURCE_SHOW,
		        xmpp_presence_show[resource->show]);
		status_str = g_strdup(resource->status);
		if (status_str != NULL)
			stroneline(status_str);
		status = (resource->status == NULL) ? NULL :
		    format_get_text(MODULE_NAME, NULL, server, NULL,
		        XMPPTXT_FORMAT_RESOURCE_STATUS, status_str);
		g_free(status_str);
		priority = g_strdup_printf("%d", resource->priority);
		text = format_get_text(MODULE_NAME, NULL, server, NULL,
		    XMPPTXT_FORMAT_RESOURCE, show, resource->name, priority,
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
show_user(XMPP_SERVER_REC *server, XMPP_ROSTER_USER_REC *user)
{
	const char *first_show;
	char *name, *resources, *subscription;

	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(user != NULL);
	if (user->resources == NULL)
		first_show = xmpp_presence_show[user->error ?
		    XMPP_PRESENCE_ERROR : XMPP_PRESENCE_UNAVAILABLE];
	else
		first_show = get_first_show(user->resources);
	name = user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, user->jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, user->jid);
	resources = get_resources(server, user->resources);
	subscription = user->subscription == XMPP_SUBSCRIPTION_BOTH ? NULL :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_SUBSCRIPTION,
	        xmpp_subscription[user->subscription]);
	printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
    	    XMPPTXT_ROSTER_CONTACT, first_show, name, resources,
	    subscription);
	g_free(name);
	g_free(resources);
	g_free(subscription);
}

static void
show_begin_of_roster(XMPP_SERVER_REC *server)
{
	char *show, *status, *priority, *text, *resources;

	g_return_if_fail(IS_XMPP_SERVER(server));
	show = (server->show == XMPP_PRESENCE_AVAILABLE) ? NULL :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_RESOURCE_SHOW,
	    xmpp_presence_show[server->show]);
	status = (server->away_reason == NULL
	    || strcmp(server->away_reason, " ") == 0) ? NULL :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_RESOURCE_STATUS, server->away_reason);
	priority = g_strdup_printf("%d", server->priority);
	text = format_get_text(MODULE_NAME, NULL, server, NULL,
	    XMPPTXT_FORMAT_RESOURCE, show,  server->resource, priority,
	    status);
	g_free(show);
	g_free(status);
	g_free(priority);
	resources = get_resources(server, server->my_resources);
	printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
	    XMPPTXT_BEGIN_OF_ROSTER, server->jid, text, resources);
	g_free(text);
	g_free(resources);
}

static void
sig_roster_show(XMPP_SERVER_REC *server)
{
	GSList *gl, *ul;
	XMPP_ROSTER_GROUP_REC *group;
	XMPP_ROSTER_USER_REC *user;

	g_return_if_fail(IS_XMPP_SERVER(server));
	show_begin_of_roster(server);
	for (gl = server->roster; gl != NULL; gl = gl->next) {
		group = gl->data;
		/* don't show groups with only offline users */
		for (ul = group->users; ul != NULL
		    && !user_is_shown(ul->data); ul = ul->next);
		if (ul == NULL)
			continue;
		show_group(server, group);
		 for (ul = group->users; ul != NULL; ul = ul->next) {
			user = ul->data;
			if (user_is_shown(user))
				show_user(server, user);
		}
	}
	printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
	    XMPPTXT_END_OF_ROSTER);
}

static void
sig_not_in_roster(XMPP_SERVER_REC *server, const char *jid)
{
	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(jid != NULL);
	printformat_module(MODULE_NAME, server, NULL,
	    MSGLEVEL_CLIENTERROR, XMPPTXT_NOT_IN_ROSTER, jid);
}

static void
sig_subscribe(XMPP_SERVER_REC *server, const char *jid, const char *status)
{
	XMPP_ROSTER_USER_REC *user;
	char *name;

	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(jid != NULL);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);
	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBE, name, status);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBE, name, status);
	g_free(name);
}

static void
sig_subscribed(XMPP_SERVER_REC *server, const char *jid)
{
	XMPP_ROSTER_USER_REC *user;
	char *name;

	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(jid != NULL);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);
	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBED, name);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_SUBSCRIBED, name);
	g_free(name);
}

static void
sig_unsubscribe(XMPP_SERVER_REC *server, const char *jid)
{
	XMPP_ROSTER_USER_REC *user;
	char *name;

	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(jid != NULL);

	user = rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);
	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBE, name);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBE, name);
	g_free(name);
}

static void
sig_unsubscribed(XMPP_SERVER_REC *server, const char *jid)
{
	XMPP_ROSTER_USER_REC *user;
	char *name;

	g_return_if_fail(IS_SERVER(server));
	g_return_if_fail(jid != NULL);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);
	if (settings_get_bool("xmpp_status_window"))
		printformat_module_window(MODULE_NAME,
		    fe_xmpp_status_get_window(server), MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBED, name);
	else
		printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
		    XMPPTXT_UNSUBSCRIBED, name);
	g_free(name);
}

void
fe_rosters_init(void)
{
	signal_add("xmpp roster show", sig_roster_show);
	signal_add("xmpp not in roster", sig_not_in_roster);
	signal_add("xmpp presence subscribe", sig_subscribe);
	signal_add("xmpp presence subscribed", sig_subscribed);
	signal_add("xmpp presence unsubscribe", sig_unsubscribe);
	signal_add("xmpp presence unsubscribed", sig_unsubscribed);

	settings_add_str("xmpp_roster", "xmpp_roster_default_group", "General");
	settings_add_str("xmpp_roster", "xmpp_roster_service_name",
	    "Agents/Services");
	settings_add_bool("xmpp_roster", "xmpp_roster_show_offline", TRUE);
	settings_add_bool("xmpp_roster", "xmpp_roster_show_unsubscribed", TRUE);
}

void
fe_rosters_deinit(void)
{
	signal_remove("xmpp roster show", sig_roster_show);
	signal_remove("xmpp not in roster",  sig_not_in_roster);
	signal_remove("xmpp presence subscribe", sig_subscribe);
	signal_remove("xmpp presence subscribed", sig_subscribed);
	signal_remove("xmpp presence unsubscribe", sig_unsubscribe);
	signal_remove("xmpp presence unsubscribed", sig_unsubscribed);
}
