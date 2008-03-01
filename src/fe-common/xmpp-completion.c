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
#include "channels-setup.h"
#include "misc.h"
#include "settings.h"
#include "signals.h"
#include "window-items.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-channels.h"
#include "xmpp-commands.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

static char *
quoted_if_space(const char *name, const char *res)
{
	if (res != NULL)
		return g_utf8_strchr(res, -1, ' ') == NULL ?
		    g_strconcat(name, "/", res, NULL) :
		    g_strconcat("\"", name, "/", res, "\"", NULL);
	else
		return g_utf8_strchr(name, -1, ' ') == NULL ?
		    g_strdup(name) : g_strconcat("\"", name, "\"", NULL);
}

static GList *
get_resources(XMPP_SERVER_REC *server, const char *nick,
    const char *resource_name, gboolean quoted)
{
	GSList *rl;
	GList *list;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	size_t len;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(nick != NULL, NULL);

	if (resource_name != NULL)
		len = strlen(resource_name);

	list = NULL;

	user = xmpp_rosters_find_user(server->roster, nick, NULL);
	if (user == NULL)
		return NULL;

	for(rl = user->resources; rl != NULL; rl = rl->next) {
		resource = rl->data;
		if (resource_name == NULL
		    || g_strncasecmp(resource->name, resource_name, len) == 0)
			list = g_list_append(list, quoted ?
			    quoted_if_space(nick, resource->name) :
			    g_strconcat(nick, "/", resource->name, NULL));
	}

	return list;
}

static GList *
get_nicks(XMPP_SERVER_REC *server, const char *nick, gboolean quoted,
    gboolean complete_names)
{
	GSList *gl, *ul;
	GList *list;
	XMPP_ROSTER_USER_REC *user;
	char *jid, *resource;
	int len;
	gboolean pass2;
	
	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(nick != NULL, NULL);

	len = strlen(nick);

	/* resources completion */
	resource = xmpp_extract_resource(nick);
	if (resource != NULL) {
		jid = xmpp_strip_resource(nick);

		list = get_resources(server, jid, resource, quoted);

		g_free(resource);
		g_free(jid);

		return list;
	}

	list = NULL;
	pass2 = FALSE;

again:
	/* first complete with online contacts
	 * then complete with offline contacts */
	for (gl = server->roster; gl != NULL; gl = gl->next) {

		for (ul = ((XMPP_ROSTER_GROUP_REC *)gl->data)->users;
		    ul != NULL ; ul = ul->next) {
			user = (XMPP_ROSTER_USER_REC *)ul->data;

			if ((!pass2 && user->resources == NULL)
			    || (pass2 && user->resources != NULL))
			    	continue;

			if (complete_names && user->name != NULL
			    && g_strncasecmp(user->name, nick, len) == 0)
				list = g_list_prepend(list, quoted ?
				    quoted_if_space(user->name, NULL) :
				    g_strdup(user->name));

			if (g_strncasecmp(user->jid, nick, len) == 0)
				list = g_list_prepend(list, quoted ?
				    quoted_if_space(user->jid, NULL) :
				    g_strdup(user->jid));
		}
	}
	
	if ((pass2 = !pass2))
		goto again;

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

	if (g_ascii_strncasecmp(linestart,
	    settings_get_str("cmdchars"), 1) == 0) {
		*list = g_list_concat(*list, get_nicks(server, *word == '"' ?
		    word+1 : word , TRUE, TRUE));

	} else if (!IS_XMPP_CHANNEL(window->active))
		*list = g_list_concat(*list, get_nicks(server, word, FALSE,
		    TRUE));
}

static void
sig_complete_command_roster_group(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	GSList *gl;
	XMPP_SERVER_REC *server;
	XMPP_ROSTER_GROUP_REC *group;
	int len;
	char **tmp;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);
	g_return_if_fail(args != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	len = strlen(word);
	tmp = g_strsplit(args, " ", 2);

	/* complete nicks */
	if (tmp[0] == NULL)
		*list = g_list_concat(*list, get_nicks(server, *word == '"' ?
		    word+1 : word , TRUE, FALSE));

	/* complete groups */
	else if (tmp[0] != NULL && tmp[1] == NULL) {
		for (gl = server->roster; gl != NULL; gl = gl->next) {
			group = (XMPP_ROSTER_GROUP_REC *)gl->data;
			
			if (group->name != NULL &&
			    g_ascii_strncasecmp(word, group->name, len) == 0)
				*list = g_list_append(*list,
				    g_strdup(group->name));
		}
	}

	g_strfreev(tmp);

	if (*list != NULL)
		signal_stop();
}

static void
sig_complete_command_roster_others(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	GSList *gl;
	XMPP_SERVER_REC *server;
	XMPP_ROSTER_GROUP_REC *group;
	int len;
	char **tmp;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);
	g_return_if_fail(args != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	len = strlen(word);
	tmp = g_strsplit(args, " ", 2);

	/* complete nicks */
	if (tmp[0] == NULL)
		*list = g_list_concat(*list, get_nicks(server, *word == '"' ?
		    word+1 : word , TRUE, FALSE));

	g_strfreev(tmp);

	if (*list != NULL)
		signal_stop();
}

static GList *
get_channels(XMPP_SERVER_REC *server, const char *word)
{
	GSList *tmp;
	GList *list;
	XMPP_CHANNEL_REC *channel;
	CHANNEL_SETUP_REC *channel_setup;
	int len;
	
	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(word != NULL, NULL);

	len = strlen(word);
	list = NULL;

	/* get joined channels */
	for (tmp = server->channels; tmp != NULL; tmp = tmp->next) {
		channel = XMPP_CHANNEL(tmp->data);

		if (channel != NULL &&
		    g_strncasecmp(channel->name, word, len) == 0)
			list = g_list_append(list, g_strdup(channel->name));
	}

	/* get channels from setup */
	for (tmp = setupchannels; tmp != NULL; tmp = tmp->next) {
		channel_setup = tmp->data;

		if ((IS_XMPP_CHANNEL_SETUP(channel_setup)
		    || *channel_setup->name != '#')
		    && g_strncasecmp(channel_setup->name, word, len) == 0
		    && glist_find_string(list, channel_setup->name) == NULL)
			list = g_list_append(list,
			    g_strdup(channel_setup->name));
	}

	return list;
}

static void
sig_complete_command_channels(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	XMPP_SERVER_REC *server;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	*list = get_channels(server, word);

	if (*list != NULL)
		signal_stop();
}

static void
sig_complete_command_invite(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	XMPP_SERVER_REC *server;
	char **tmp;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	/* complete channels */
	tmp = g_strsplit(args, " ", 2);
	if (tmp[0] != NULL && tmp[1] == NULL)
		*list = get_channels(server, word);
	g_strfreev(tmp);

	if (*list != NULL)
		signal_stop();
}

static void
sig_complete_command_away(GList **list, WINDOW_REC *window,
    const char *word, const char *args, int *want_space)
{
	XMPP_SERVER_REC *server;
	int len;

	g_return_if_fail(list != NULL);
	g_return_if_fail(window != NULL);
	g_return_if_fail(word != NULL);

	server = XMPP_SERVER(window->active_server);
	if (server == NULL)
		return;

	len = strlen(word);

	if (g_strncasecmp(word,
	    xmpp_presence_show[XMPP_PRESENCE_AWAY], len) == 0)
		*list = g_list_append(*list,
		    g_strdup(xmpp_presence_show[XMPP_PRESENCE_AWAY]));

	if (g_strncasecmp(word,
	    xmpp_presence_show[XMPP_PRESENCE_XA], len) == 0)
		*list = g_list_append(*list,
		    g_strdup(xmpp_presence_show[XMPP_PRESENCE_XA]));

	if (g_strncasecmp(word,
	    xmpp_presence_show[XMPP_PRESENCE_DND], len) == 0)
		*list = g_list_append(*list,
		    g_strdup(xmpp_presence_show[XMPP_PRESENCE_DND]));

	if (g_strncasecmp(word,
	    xmpp_presence_show[XMPP_PRESENCE_CHAT], len) == 0)
		*list = g_list_append(*list,
		    g_strdup(xmpp_presence_show[XMPP_PRESENCE_CHAT]));

	if (g_strncasecmp(word,
	    xmpp_presence_show[XMPP_PRESENCE_ONLINE_STR], len) == 0)
		*list = g_list_append(*list, g_strdup("online"));

	signal_stop();
}

void
xmpp_completion_init(void)
{
	signal_add("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_add("complete command roster group",
	    (SIGNAL_FUNC)sig_complete_command_roster_group);
	signal_add("complete command roster add",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster remove",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster name",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster accept",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster deny",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster subscribe",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command roster unsubscribe",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_add("complete command join",
	    (SIGNAL_FUNC)sig_complete_command_channels);
	signal_add("complete command part",
	    (SIGNAL_FUNC)sig_complete_command_channels);
	signal_add("complete command invite",
	    (SIGNAL_FUNC)sig_complete_command_invite);
	signal_add("complete command away",
	    (SIGNAL_FUNC)sig_complete_command_away);
}

void
xmpp_completion_deinit(void)
{
	signal_remove("complete word", (SIGNAL_FUNC)sig_complete_word);
	signal_remove("complete command roster group",
	    (SIGNAL_FUNC) sig_complete_command_roster_group);
	signal_remove("complete command roster add",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster remove",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster name",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster accept",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster deny",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster subscribe",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command roster unsubscribe",
	    (SIGNAL_FUNC)sig_complete_command_roster_others);
	signal_remove("complete command join",
	    (SIGNAL_FUNC)sig_complete_command_channels);
	signal_remove("complete command part",
	    (SIGNAL_FUNC)sig_complete_command_channels);
	signal_remove("complete command invite",
	    (SIGNAL_FUNC)sig_complete_command_invite);
	signal_remove("complete command away",
	    (SIGNAL_FUNC)sig_complete_command_away);
}
