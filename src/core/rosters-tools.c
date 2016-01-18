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
#include <stdlib.h>

#include "module.h"

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

XMPP_ROSTER_GROUP_REC *
find_group_from_user(XMPP_SERVER_REC *server, XMPP_ROSTER_USER_REC *user)
{
	GSList *gl, *gl_found;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	gl = server->roster;
	gl_found = NULL;
	while (gl_found != NULL && gl != NULL) {
		gl_found = g_slist_find(gl, user);
		gl = gl->next;
	}
	return (XMPP_ROSTER_GROUP_REC *)gl->data;
}

XMPP_ROSTER_USER_REC *
rosters_find_user(GSList *groups, const char *jid,
    XMPP_ROSTER_GROUP_REC **group, XMPP_ROSTER_RESOURCE_REC **resource)
{
	GSList *group_tmp, *gl, *ul;
	char *pos;

	if ((pos = xmpp_find_resource_sep(jid)) != NULL)
		*pos = '\0';
	group_tmp = ul = NULL;
	for (gl = groups; ul == NULL && gl != NULL;
	    gl = gl->next) {
		ul = g_slist_find_custom(
		    ((XMPP_ROSTER_GROUP_REC *)gl->data)->users, jid,
		    find_user_func);
		group_tmp = gl;
	}
	if (group != NULL)
		*group = ul != NULL ?
		    (XMPP_ROSTER_GROUP_REC *)group_tmp->data : NULL;
	if (resource != NULL)
		*resource = ul != NULL && pos != NULL ?
		    rosters_find_resource(
		    ((XMPP_ROSTER_USER_REC *)ul->data)->resources, pos+1)
		    : NULL;
	if (pos != NULL)
		*pos = '/';
	return ul != NULL ?
	    (XMPP_ROSTER_USER_REC *)ul->data : NULL;
}

XMPP_ROSTER_USER_REC *
find_username(GSList *groups, const char *name, XMPP_ROSTER_GROUP_REC **group)
{
	GSList *gl, *group_tmp, *ul;

	gl = groups;
	group_tmp = ul = NULL;
	while (ul == NULL && gl != NULL) {
		ul = g_slist_find_custom(
		    ((XMPP_ROSTER_GROUP_REC *)gl->data)->users, name,
		    find_username_func);
		group_tmp = gl;
		gl = g_slist_next(gl);
	}
	if (group != NULL && group_tmp != NULL)
		*group = group_tmp->data;
	return ul ? (XMPP_ROSTER_USER_REC *)ul->data : NULL;
}

XMPP_ROSTER_RESOURCE_REC *
rosters_find_resource(GSList *resources, const char *res)
{
	GSList *resource;

	if (resources == NULL)
		return NULL;
	resource = g_slist_find_custom(resources, res, find_resource_func);
	return resource != NULL ?
	    (XMPP_ROSTER_RESOURCE_REC *)resource->data : NULL;
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
				res = resource->name;
				if (res != NULL && *res != '\0')
					return g_strconcat(user->jid, "/",
					    res, (void *)NULL);
			}
			return g_strdup(user->jid);
		}
		res = xmpp_extract_resource(name);
		str = g_strconcat(user->jid, "/", res, (void *)NULL);
		g_free(res);
		return str;
	}
	return NULL;
}

char *
rosters_get_name(XMPP_SERVER_REC *server, const char *full_jid)
{
	GSList *gl, *ul;
	XMPP_ROSTER_GROUP_REC *group;
	XMPP_ROSTER_USER_REC *user;
	char *jid;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(full_jid != NULL, NULL);
	if ((jid = xmpp_strip_resource(full_jid)) == NULL)
		return NULL;
	for (gl = server->roster; gl != NULL; gl = gl->next) {
		group = gl->data;
		for (ul = group->users; ul != NULL; ul = ul->next) {
			user = ul->data;
			if (strcmp(jid, user->jid) == 0) {
				g_free(jid);
				return user->name;
			}
		}
	}
	g_free(jid);
	return NULL;
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
