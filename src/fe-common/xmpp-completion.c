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
#include "signals.h"
#include "window-items.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-commands.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

static GList *
get_resources(XMPP_SERVER_REC *server, const char *nick,
    const char *resource_name)
{
	GSList *resource_list;
	GList *list;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	size_t len;

	g_return_val_if_fail(nick != NULL, NULL);
	if (!IS_XMPP_SERVER(server))
		return NULL;

	if (resource_name)
		len = strlen(resource_name);

	list = NULL;

	user = xmpp_rosters_find_user(server->roster, nick, NULL);
	if (user == NULL)
		return NULL;

	for(resource_list = user->resources; resource_list != NULL;
	    resource_list = resource_list->next) {

		resource = (XMPP_ROSTER_RESOURCE_REC *)resource_list->data;
		if ((resource_name == NULL
		    || g_strncasecmp(resource->name, resource_name, len) == 0)
		    && resource->name != NULL) {
			
			list = g_list_append(list,
			    (gpointer)g_strdup_printf("%s/%s", nick,
			    resource->name));
		}
	}

	return list;
}

static GList *
get_nicks(XMPP_SERVER_REC *server, const char *nick)
{
	GSList *group_list, *user_list;
	GList *list;
	XMPP_ROSTER_USER_REC *user;
	gchar *jid, *resource;
	int len;
	
	g_return_val_if_fail(server != NULL, NULL);
	g_return_val_if_fail(nick != NULL, NULL);

	len = strlen(nick);

	/* resources completion */
	resource = xmpp_extract_resource(nick);
	if (resource != NULL) {
		jid = xmpp_strip_resource(nick);

		list = get_resources(server, jid, NULL);

		g_free(resource);
		g_free(jid);

		return list;
	}

	list = NULL;

	for (group_list = server->roster; group_list != NULL;
	    group_list = group_list->next) {

		for (user_list =
		    ((XMPP_ROSTER_GROUP_REC *)group_list->data)->users;
		    user_list != NULL ; user_list = user_list->next) {

			user = (XMPP_ROSTER_USER_REC *)user_list->data;

			if (user->name != NULL
			    && g_utf8_strchr(user->name, -1, ' ') == NULL
			    && g_strncasecmp(user->name, nick, len) == 0)
				list = g_list_prepend(list,
				    (gpointer)g_strdup(user->name));

			if (g_strncasecmp(user->jid, nick, len) == 0)
				list = g_list_append(list,
				    (gpointer)g_strdup(user->jid));
		}
	}

	return list;
}

static void
sig_complete_word(GList **list, WINDOW_REC *window, const char *word,
    const char *linestart, int *want_space)
{
	XMPP_SERVER_REC *server;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	if (!IS_XMPP_CHANNEL(window->active) ||
	    g_ascii_strcasecmp(linestart, "/Q") == 0) {
		g_list_free(*list);
		*list = get_nicks(server, word);
	}
}

static void
sig_complete_command_roster_group(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	GSList *group_list;
	XMPP_SERVER_REC *server;
	XMPP_ROSTER_GROUP_REC *group;
	int len;
	char **tmp;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);
	g_return_if_fail(args != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL || *args == '\0')
		return;

	g_list_free(*list);
	*list = NULL;
	len = strlen(word);
	tmp = g_strsplit(args, " ", 2);

	/* complete groups */
	if (tmp[0] != NULL && tmp[1] == NULL) {

		for (group_list = server->roster; group_list != NULL;
		    group_list = group_list->next) {
			group = (XMPP_ROSTER_GROUP_REC *)group_list->data;
			if (group->name != NULL &&
			    g_ascii_strncasecmp(word, group->name, len) == 0)
				*list = g_list_append(*list,
				    (gpointer)g_strdup(group->name));
		}

	}
	g_strfreev(tmp);
}

void
xmpp_completion_init(void)
{
	signal_add("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_add("complete command roster group",
	    (SIGNAL_FUNC)sig_complete_command_roster_group);
}

void
xmpp_completion_deinit(void)
{
	signal_remove("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_remove("complete command roster group",
	    (SIGNAL_FUNC) sig_complete_command_roster_group);
}
