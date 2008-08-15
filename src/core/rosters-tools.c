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
#include <stdlib.h>

#include "module.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "tools.h"

static int
find_user_func(gconstpointer user, gconstpointer jid)
{
	g_return_val_if_fail(user != NULL, -1);
	g_return_val_if_fail(jid != NULL, -1);
	return strcmp(((XMPP_ROSTER_USER_REC *)user)->jid, jid);
}

static int
find_username_func(gconstpointer user_pointer, gconstpointer name)
{
	XMPP_ROSTER_USER_REC *user;

	g_return_val_if_fail(user_pointer != NULL, -1);
	user = (XMPP_ROSTER_USER_REC *)user_pointer;
	if (user->name == NULL)
		return -1;
	return strcmp(user->name, name);
}

static int
find_resource_func(gconstpointer resource, gconstpointer name)
{
	char *res;

	g_return_val_if_fail(resource != NULL, -1);
	res = ((XMPP_ROSTER_RESOURCE_REC *)resource)->name;
	if(res == NULL && name == NULL)
		return 0;
	if (res == NULL || name == NULL)
		return -1;
	return strcmp(res, name);
}

static int
func_sort_user(gconstpointer user1_pointer, gconstpointer user2_pointer)
{
	GSList *resources1_list, *resources2_list;
	XMPP_ROSTER_USER_REC *user1, *user2;
	XMPP_ROSTER_RESOURCE_REC *fisrt_resources1, *fisrt_resources2;

	user1 = (XMPP_ROSTER_USER_REC *)user1_pointer;
	resources1_list = user1->resources;
	user2 = (XMPP_ROSTER_USER_REC *)user2_pointer;
	resources2_list = user2->resources;
	if (resources1_list == resources2_list
	    || (user1->error == TRUE && user2->error == TRUE))
		goto by_name;
	if (user1->error || resources1_list == NULL)
		return 1;
	if (user2->error || resources2_list == NULL)
		return -1;
	fisrt_resources1 = (XMPP_ROSTER_RESOURCE_REC *)resources1_list->data;
	fisrt_resources2 = (XMPP_ROSTER_RESOURCE_REC *)resources2_list->data;
	if (fisrt_resources1->show == fisrt_resources2->show)
		goto by_name;
	return fisrt_resources2->show - fisrt_resources1->show;

by_name:
	if (user1->name == NULL && user2->name != NULL)
		return -1;
	if (user1->name != NULL && user2->name == NULL)
		return 1;
	if (user1->name != NULL && user2->name != NULL)
		return strcmp(user1->name, user2->name);
	return strcmp(user1->jid, user2->jid);
}

XMPP_ROSTER_GROUP_REC *
find_group_from_user(XMPP_SERVER_REC *server, XMPP_ROSTER_USER_REC *user)
{
	GSList *group_list, *group_list_found;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	group_list = server->roster;
	group_list_found = NULL;
	while (group_list_found != NULL && group_list != NULL) {
		group_list_found = g_slist_find(group_list, user);
		group_list = group_list->next;
	}
	return (XMPP_ROSTER_GROUP_REC *)group_list->data;
}

XMPP_ROSTER_USER_REC *
rosters_find_user(GSList *groups, const char *jid,
    XMPP_ROSTER_GROUP_REC **group, XMPP_ROSTER_RESOURCE_REC **resource)
{
	GSList *group_tmp, *group_list, *user_list;
	char *pos;

	if ((pos = xmpp_find_resource_sep(jid)) != NULL)
		*pos = '\0';
	user_list = NULL;
	for (group_list = groups; user_list == NULL && group_list != NULL;
	    group_list = group_list->next) {
		user_list = g_slist_find_custom(
		    ((XMPP_ROSTER_GROUP_REC *)group_list->data)->users, jid,
		    find_user_func);
		group_tmp = group_list;
	}
	if (group != NULL)
		*group = user_list != NULL ?
		    (XMPP_ROSTER_GROUP_REC *)group_tmp->data : NULL;
	if (resource != NULL)
		*resource = user_list != NULL && pos != NULL ?
			rosters_find_resource(
			    (XMPP_ROSTER_USER_REC *)user_list->data, pos+1)
			    : NULL;
	if (pos != NULL)
		*pos = '/';
	return user_list != NULL ?
	    (XMPP_ROSTER_USER_REC *)user_list->data : NULL;
}

XMPP_ROSTER_USER_REC *
find_username(GSList *groups, const char *name, XMPP_ROSTER_GROUP_REC **group)
{
	GSList *group_list, *group_tmp, *user_list;

	group_list = groups;
	group_tmp = user_list = NULL;
	while (user_list == NULL && group_list != NULL) {
		user_list = g_slist_find_custom(
		    ((XMPP_ROSTER_GROUP_REC *)group_list->data)->users, name,
		    find_username_func);
		group_tmp = group_list;
		group_list = g_slist_next(group_list);
	}
	if (group != NULL && group_tmp != NULL)
		*group = group_tmp->data;
	return user_list ? (XMPP_ROSTER_USER_REC *)user_list->data : NULL;
}

XMPP_ROSTER_RESOURCE_REC *
rosters_find_resource(XMPP_ROSTER_USER_REC *user, const char *resource)
{
	GSList *resource_list;

	g_return_val_if_fail(user != NULL, NULL);
	resource_list = g_slist_find_custom(user->resources, resource,
	    find_resource_func);
	return resource_list ?
	    (XMPP_ROSTER_RESOURCE_REC *)resource_list->data : NULL;
}

XMPP_ROSTER_RESOURCE_REC *
rosters_find_own_resource(XMPP_SERVER_REC *server, const char *resource)
{
	GSList *resource_list;

	g_return_val_if_fail(server != NULL, NULL);
	resource_list = g_slist_find_custom(server->my_resources, resource,
	    find_resource_func);
	return resource_list ?
	    (XMPP_ROSTER_RESOURCE_REC *)resource_list->data : NULL;
}

char *
rosters_resolve_name(XMPP_SERVER_REC *server, const char *name)
{
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *res, *str;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(name != NULL, NULL);
	g_strstrip((char *)name);
	user = find_username(server->roster, name, NULL);
	if (user == NULL)
		user = rosters_find_user(server->roster, name, NULL, NULL);
	if (user != NULL) {
		if (!xmpp_have_resource(name)) {
			/* if unspecified, use the highest resource */
			if (user->resources != NULL) {
				resource = user->resources->data;
				if (resource->name != NULL)
					return g_strconcat(user->jid, "/",
					    resource->name, NULL);
			}
			return g_strdup(user->jid);
		}
		res = xmpp_extract_resource(name);
		str = g_strconcat(user->jid, "/", res, NULL);
		g_free(res);
		return str;
	}
	return NULL;
}

void
rosters_reorder(XMPP_ROSTER_GROUP_REC *group)
{
	g_return_if_fail(group != NULL);
	group->users = g_slist_sort(group->users, func_sort_user);
}

int
xmpp_get_show(const char *show)
{
	if (show != NULL && *show != '\0') {
		if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_CHAT]) == 0)
			return XMPP_PRESENCE_CHAT;
		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_DND]) == 0)
			return XMPP_PRESENCE_DND;
		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_XA]) == 0)
			return XMPP_PRESENCE_XA;
		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_AWAY]) == 0)
			return XMPP_PRESENCE_AWAY;
		else if (g_ascii_strcasecmp(show,
		    xmpp_presence_show[XMPP_PRESENCE_ONLINE]) == 0)
			return XMPP_PRESENCE_AVAILABLE;
	}
	return XMPP_PRESENCE_AVAILABLE;
}
