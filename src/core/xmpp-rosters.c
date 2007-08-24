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
#include <stdlib.h>

#include "module.h"
#include "signals.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"

const gchar *xmpp_presence_show[] = {
	"X",
	"-",
	"xa",
	"dnd",
	"away",
	"+",
	"chat",
	NULL
};

const gchar *xmpp_subscription[] = {
	"remove",
	"none",
	"to",
	"from",
	"both",
	NULL
};

const gchar *xmpp_service_name = "Agents/Transports";

static XmppRosterGroup *xmpp_roster_create_group(const gchar *);
static void xmpp_roster_cleanup_ressource(gpointer, gpointer); 
static void xmpp_roster_cleanup_user(gpointer, gpointer);

static int
xmpp_find_group_func(gconstpointer group, gconstpointer name)
{
	gchar *group_name = ((XmppRosterGroup *)group)->name;

	if(group_name == name)
		return 0;
	if (group_name == NULL || name == NULL)
		return -1;

	return strcmp(group_name, name);
}

static int
xmpp_sort_group_func(gconstpointer group1, gconstpointer group2)
{
	gchar *group1_name = ((XmppRosterGroup *)group1)->name;
	gchar *group2_name = ((XmppRosterGroup *)group2)->name;

	if (group1_name == NULL || (group2_name != NULL
	    && strcmp(group2_name, xmpp_service_name) == 0))
		return -1;
	if (group1_name != NULL || (group2_name == NULL
	    && strcmp(group1_name, xmpp_service_name) == 0))
		return 1;

	return strcmp(group1_name, group2_name);
}

static int
xmpp_find_user_func(gconstpointer user, gconstpointer jid)
{
	return strcmp(((XmppRosterUser *)user)->jid, jid);
}

int
xmpp_sort_user_func(gconstpointer user1_pointer, gconstpointer user2_pointer)
{
	XmppRosterUser *user1 = (XmppRosterUser *)user1_pointer;
	XmppRosterUser *user2 = (XmppRosterUser *)user2_pointer;
	GSList *ressources1_list =  user1->ressources;
	GSList *ressources2_list =  user2->ressources;
	XmppRosterRessource *fisrt_ressources1, *fisrt_ressources2;

	if (ressources1_list == ressources2_list
	    || (user1->error == TRUE && user2->error == TRUE))
		goto by_name;
	if (user1->error != NULL || ressources1_list == NULL)
		return 1;
	if (user2->error == NULL || ressources2_list != NULL)
		return -1;

	fisrt_ressources1 = (XmppRosterRessource *)ressources1_list->data;
	fisrt_ressources2 = (XmppRosterRessource *)ressources2_list->data;

	if (fisrt_ressources1->show == fisrt_ressources2->show)
		goto by_name;
		
	return fisrt_ressources2->show - fisrt_ressources1->show;

by_name:
	if (user1->name == NULL && user2->name != NULL)
		return -1;
	if (user1->name != NULL && user2->name == NULL)
		return 1;
	if (user1->name != NULL && user2->name != NULL)
		return strcmp(user1->name, user2->name);

	return strcmp(user1->jid, user2->jid);
}

static int
xmpp_find_ressource_func(gconstpointer ressource, gconstpointer name)
{
	gchar *ressource_name = ((XmppRosterRessource *)ressource)->name;

	if(ressource_name == NULL && name == NULL)
		return 0;
	if (ressource_name == NULL || name == NULL)
		return -1;
	return strcmp(ressource_name, name);
}

static int
xmpp_sort_ressource_func(gconstpointer ressource1, gconstpointer ressource2)
{
	return ((XmppRosterRessource *)ressource2)->priority
	    - ((XmppRosterRessource *)ressource1)->priority;
}

XmppRosterUser *
xmpp_find_user_from_groups(GSList *groups, const gchar *jid,
    XmppRosterGroup **group)
{
	GSList *group_list, *group_tmp, *user_list;

	group_list = groups;
	group_tmp = NULL;
	user_list = NULL;

	while (user_list == NULL && group_list != NULL) {
		user_list = g_slist_find_custom(
		    ((XmppRosterGroup *)group_list->data)->users, jid,
		    (GCompareFunc)xmpp_find_user_func);

		group_tmp = group_list;
		group_list = g_slist_next(group_list);
	}

	if (group != NULL && group_tmp != NULL)
		*group = group_tmp->data;

	return user_list ? (XmppRosterUser *)user_list->data : NULL;
}

static XmppRosterGroup *
xmpp_find_group_from_user(XMPP_SERVER_REC *server, XmppRosterUser *user)
{
	GSList *group_list, *group_list_found;

	group_list = server->roster;
	group_list_found = NULL;

	while (group_list_found != NULL && group_list != NULL) {
		group_list_found = g_slist_find(group_list, user);
		group_list = g_slist_next(group_list);
	}

	return (XmppRosterGroup *)group_list->data;
}

static XmppRosterGroup *
xmpp_find_or_add_group(XMPP_SERVER_REC *server, const gchar *group_name)
{
	GSList *group_list;
	XmppRosterGroup *group;

	group_list = g_slist_find_custom(server->roster, group_name,
	    (GCompareFunc)xmpp_find_group_func);

	/* group doesn't exist */
	if (group_list == NULL) {
		group = xmpp_roster_create_group(group_name);
		server->roster = g_slist_insert_sorted(server->roster, group,
		    (GCompareFunc)xmpp_sort_group_func);
	} else 
		group = group_list->data;

	return group;
}

static XmppRosterRessource *
xmpp_find_ressource_from_user(XmppRosterUser *user, gchar *ressource)
{
	GSList *ressource_list;

	ressource_list = g_slist_find_custom(user->ressources, ressource,
		(GCompareFunc) xmpp_find_ressource_func);

	return ressource_list ?
	    (XmppRosterRessource *)ressource_list->data : NULL;
}

gboolean
xmpp_roster_show_user(XmppRosterUser *user)
{
	if (user->ressources == NULL
	    && !settings_get_bool("roster_show_offline")
	    && (user->subscription == XMPP_SUBSCRIPTION_BOTH
	        || (user->subscription != XMPP_SUBSCRIPTION_BOTH
	            && !settings_get_bool("roster_show_offline_unsuscribed"))))
		return FALSE;
	return TRUE;
}

void
xmpp_roster_update_subscription(XMPP_SERVER_REC *server, XmppRosterUser *user,
    const gchar *subscription)
{
	XmppRosterGroup *group;

	if (user == NULL)
		return;

	if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_NONE]) == 0)

		user->subscription = XMPP_SUBSCRIPTION_NONE;

	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_FROM]) == 0)

		user->subscription = XMPP_SUBSCRIPTION_FROM;

	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_TO]) == 0)

		user->subscription = XMPP_SUBSCRIPTION_TO;

	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_BOTH]) == 0)

		user->subscription = XMPP_SUBSCRIPTION_BOTH;

	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_REMOVE]) == 0) {

		group = xmpp_find_group_from_user(server, user);
		if (group != NULL) {
			group->users = g_slist_remove(group->users, user);
			xmpp_roster_cleanup_user(user, NULL);
		}
	}
}

static void
xmpp_roster_move_user(XMPP_SERVER_REC *server, XmppRosterUser *user,
    XmppRosterGroup *group, const gchar *group_name)
{
	XmppRosterGroup *new_group;

	new_group = xmpp_find_or_add_group(server, group_name);

	group->users = g_slist_remove(group->users, user);
	new_group->users = g_slist_append(new_group->users, user);
}

static XmppRosterUser *
xmpp_roster_create_user(const gchar *jid, const gchar *name)
{
	XmppRosterUser *user = g_new(XmppRosterUser, 1);

	user->jid = g_strdup(jid);
	user->name = g_strdup(name);
	user->subscription = XMPP_SUBSCRIPTION_NONE;
	user->ressources = NULL;
	user->error = FALSE;

	return user;
}

static XmppRosterGroup *
xmpp_roster_create_group(const gchar *name)
{
	XmppRosterGroup *group = g_new(XmppRosterGroup, 1);

	group->name = g_strdup(name);
	group->users = NULL;

	return group;
}

static XmppRosterUser *
xmpp_roster_add(XMPP_SERVER_REC *server, const gchar *jid, const gchar *name,
    const gchar *group_name)
{
	GSList *group_list, *user_list;
	XmppRosterGroup *group;
	XmppRosterUser *user;

	group = xmpp_find_or_add_group(server, group_name);
	
	user = xmpp_roster_create_user(jid, name);
	group->users = g_slist_append(group->users, user);

	return user;
}

void
xmpp_roster_update_user(XMPP_SERVER_REC *server, const gchar *jid,
    const gchar *subscription, const gchar *name, const gchar *group_name)
{
	XmppRosterUser *user;
	XmppRosterGroup *group;

	user = xmpp_find_user_from_groups(server->roster, jid, &group);
	if (user == NULL)
		user = xmpp_roster_add(server, jid, name, group_name);
	else {

		/* move user to another group */
		if ((group->name != group_name)
		    || (group->name == NULL && group_name != NULL)
		    || (group->name != NULL && group_name == NULL)
		    || (group->name != NULL && group_name != NULL
		    && strcmp(group->name, group_name) != 0))
			xmpp_roster_move_user(server, user, group, group_name);

		g_free(user->name);
		user->name = g_strdup(name);
	}

	xmpp_roster_update_subscription(server, user, subscription);
}

void
xmpp_roster_update(XMPP_SERVER_REC *server, LmMessageNode *query)
{
	LmMessageNode *item, *group_node;
	const gchar *subscription, *jid, *name;
	gchar *jid_recoded, *name_recoded, *group_name;

	name_recoded = NULL;
	group_name = NULL;

	item = query->children;
	while(item) {

		if (g_ascii_strcasecmp(item->name, "item") != 0)
			goto next;
		
		jid = lm_message_node_get_attribute(item, "jid");
		if (jid == NULL)
			goto next;
		jid_recoded = xmpp_recode(jid, XMPP_RECODE_IN);

		subscription = lm_message_node_get_attribute(item,
		    "subscription");

		name = lm_message_node_get_attribute(item, "name");
		if (name != NULL)
			name = xmpp_recode(name, XMPP_RECODE_IN);

		group_node = lm_message_node_get_child(item, "group");
		if (group_node != NULL)
			group_name = xmpp_recode(group_node->value,
			    XMPP_RECODE_IN);

		xmpp_roster_update_user(server, jid_recoded, subscription,
		    name_recoded, group_name);

		g_free_and_null(jid_recoded);
		g_free_and_null(name_recoded);
		g_free_and_null(group_name);
	
next:
		item = item->next;
	}
}

void
xmpp_roster_presence_update(XMPP_SERVER_REC *server, const gchar *full_jid,
    const gchar *show, const gchar *status, const gchar *priority)
{
	XmppRosterUser *user;
	XmppRosterRessource *ressource;
	gchar *jid, *ressource_jid;

	jid = xmpp_jid_strip_ressource(full_jid);
	ressource_jid = xmpp_jid_get_ressource(full_jid);
	
	user = xmpp_find_user_from_groups(server->roster, jid, NULL);
	if (user == NULL)
		return;
	user->error = FALSE;

	/* find ressource or create it if it doesn't exist */	
	ressource = xmpp_find_ressource_from_user(user, ressource_jid);
	if (ressource == NULL) {
		ressource = g_new(XmppRosterRessource, 1);
		ressource->name = g_strdup(ressource_jid);
		ressource->status = NULL;
		user->ressources = g_slist_prepend(user->ressources, ressource);
	}

	if (show != NULL) {
		if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_CHAT]) == 0)
			ressource->show = XMPP_PRESENCE_CHAT;

		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_DND]) == 0)
			ressource->show = XMPP_PRESENCE_DND;

		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_XA]) == 0)
			ressource->show = XMPP_PRESENCE_XA;

		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_AWAY]) == 0)
			ressource->show = XMPP_PRESENCE_AWAY;

		else
			ressource->show = XMPP_PRESENCE_AVAILABLE;
	} else
		ressource->show = XMPP_PRESENCE_AVAILABLE;

	g_free_and_null(ressource->status);
	if (status != NULL)
		ressource->status = g_strdup(status);

	if (priority)
		ressource->priority = atoi(priority);
	else
		ressource->priority = 0;

	user->ressources = g_slist_sort(user->ressources,
	    (GCompareFunc)xmpp_sort_ressource_func);

	signal_emit("xmpp jid presence change", 4, server, full_jid,
	    ressource->show, ressource->status);

	g_free(jid);
	g_free(ressource_jid);
}

void
xmpp_roster_presence_error(XMPP_SERVER_REC *server, const gchar *full_jid)
{
	XmppRosterUser *user;
	gchar *jid;

	jid = xmpp_jid_strip_ressource(full_jid);

	user = xmpp_find_user_from_groups(server->roster, jid, NULL);
	if (user == NULL)
		return;

	user->error = TRUE;

	g_free(jid);
}

void
xmpp_roster_presence_unavailable(XMPP_SERVER_REC *server,
    const gchar *full_jid, const gchar *status)
{
	gchar *jid, *ressource_jid;
	GSList *ressource_list;
	XmppRosterUser *user;
	XmppRosterRessource *ressource;

	jid = xmpp_jid_strip_ressource(full_jid);
	ressource_jid = xmpp_jid_get_ressource(full_jid);
	
	user = xmpp_find_user_from_groups(server->roster, jid, NULL);
	if (!user)
		return;

	ressource = xmpp_find_ressource_from_user(user, ressource_jid);
	if (!ressource)
		return;

	signal_emit("xmpp jid presence unavailable", 2, server, full_jid);

	user->ressources = g_slist_remove(user->ressources, ressource);
	xmpp_roster_cleanup_ressource(ressource, NULL);

	g_free(jid);
	g_free(ressource_jid);
}

static void
xmpp_roster_cleanup_ressource(gpointer data, gpointer user_data)
{
	XmppRosterRessource *ressource;
	
	if (data == NULL)
		return;
	ressource = (XmppRosterRessource *)data;

	g_free(ressource->name);
	g_free(ressource->status);
	g_free(ressource);
}

static void
xmpp_roster_cleanup_user(gpointer data, gpointer user_data)
{
	XmppRosterUser *user;
   
	if (data == NULL)
		return;
	user = (XmppRosterUser *)data;

	g_free(user->name);
	g_free(user->jid);
	g_slist_foreach(user->ressources, (GFunc)xmpp_roster_cleanup_ressource,
	    NULL);
	g_slist_free(user->ressources);
	g_free(user);
}

static void
xmpp_roster_cleanup_group(gpointer data, gpointer user_data)
{
	XmppRosterGroup *group;

	if (data == NULL)
		return;
	group = (XmppRosterGroup *)data;

	g_free(group->name);
	g_slist_foreach(group->users, (GFunc)xmpp_roster_cleanup_user, NULL);
	g_slist_free(group->users);
	g_free(group);
}

void
xmpp_roster_cleanup(XMPP_SERVER_REC *server)
{
	if (!server->roster)
		return;

	g_slist_foreach(server->roster, (GFunc)xmpp_roster_cleanup_group, NULL);
	g_slist_free(server->roster);
	server->roster = NULL;
}
