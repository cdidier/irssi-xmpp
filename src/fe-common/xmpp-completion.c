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
#include "signals.h"
#include "window-items.h"

#include "xmpp-servers.h"
#include "xmpp-rosters.h"
#include "xmpp-commands.h"
#include "xmpp-tools.h"

#include <string.h>

static GList *
completion_get_ressource_from_roster(XMPP_SERVER_REC *server, const char *nick,
    const char *ressource_name)
{
	GSList *ressource_list;
	GList *list;
	XmppRosterUser *user;
	XmppRosterRessource *ressource;
	int len;

	g_return_val_if_fail(nick != NULL, NULL);
	if (!IS_XMPP_SERVER(server))
		return NULL;

	if (ressource_name)
		len = strlen(ressource_name);

	list = NULL;

	user = xmpp_find_user_from_groups(server->roster, nick, NULL);
	if (user == NULL)
		return NULL;

	for(ressource_list = user->ressources; ressource_list != NULL;
	    ressource_list = ressource_list->next) {

		ressource = (XmppRosterRessource *)ressource_list->data;
		if ((ressource_name == NULL
		    || g_strncasecmp(ressource->name, ressource_name, len) == 0)
		    && ressource->name != NULL) {
			
			list = g_list_append(list,
			    (gpointer)g_strdup_printf("%s/%s", nick,
			    ressource->name));
		}
	}

	return list;
}

static GList *
completion_get_nick_from_roster(SERVER_REC *server, const char *nick)
{
	XMPP_SERVER_REC *xmppserver;
	GSList *group_list, *user_list;
	GList *list;
	XmppRosterUser *user;
	gchar *jid, *ressource;
	int len;
	
	g_return_val_if_fail(nick != NULL, NULL);
	xmppserver = XMPP_SERVER(server);
	if (xmppserver == NULL)
		return NULL;

	len = strlen(nick);

	/* ressources completion */
	ressource = xmpp_jid_get_ressource(nick);
	if (ressource != NULL) {
		jid = xmpp_jid_strip_ressource(nick);

		list = completion_get_ressource_from_roster(xmppserver,
		    jid, NULL);

		g_free(ressource);
		g_free(jid);

		return list;
	}

	list = NULL;

	for (group_list = xmppserver->roster; group_list != NULL;
	    group_list = group_list->next) {

		for (user_list = ((XmppRosterGroup *)group_list->data)->users;
		    user_list != NULL ; user_list = user_list->next) {

			user = (XmppRosterUser *)user_list->data;
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
	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);

	if (window->active_server)
		*list = completion_get_nick_from_roster(window->active_server,
		    word);
}

static void
sig_complete_command_roster(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	int len, i;
	char **tmp;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);
	g_return_if_fail(args != NULL);

	*list = NULL;
	len = strlen(word);

	/* complete args */
	if (args[0] != '\0') {
	
		tmp = g_strsplit(args, " ", 3);

		/* complete groups */
		if (tmp[0] != NULL && tmp[1] != NULL && tmp[2] == NULL
		    && g_ascii_strcasecmp(tmp[0],
		    xmpp_command_roster[XMPP_COMMAND_ROSTER_PARAM_GROUP])
		    == 0) {
        		GSList *group_list;
			XMPP_SERVER_REC *xmppserver;
			XmppRosterGroup *group;

			xmppserver = XMPP_SERVER(window->active_server);
			if (xmppserver == NULL)
				goto out;

			for (group_list = xmppserver->roster;
			    group_list != NULL; group_list = group_list->next) {

				group = (XmppRosterGroup *)group_list->data;
				if (group->name != NULL
				    && g_ascii_strncasecmp(word, group->name,
				    len) == 0)
					*list = g_list_append(*list,
					    (gpointer)g_strdup(group->name));
			}

		}

out:
		g_strfreev(tmp);

	/* complete subcommand */
	} else {
		for (i=0; xmpp_command_roster[i] != NULL; i++) {
			if (g_ascii_strncasecmp(word, xmpp_command_roster[i],
			    len) == 0)
				*list = g_list_append(*list,
				    (gpointer)g_strdup(xmpp_command_roster[i]));
		}
	}
}

void
xmpp_completion_init(void)
{
	signal_add("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_add("complete command roster",
	    (SIGNAL_FUNC)sig_complete_command_roster);
}

void
xmpp_completion_deinit(void)
{
	signal_remove("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_remove("complete command roster",
	    (SIGNAL_FUNC) sig_complete_command_roster);
}
