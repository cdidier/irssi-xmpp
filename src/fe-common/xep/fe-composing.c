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

#include "module.h"
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/core/special-vars.h>
#include <irssi/src/fe-common/core/window-items.h>

#include "xmpp-servers.h"
#include "xmpp-queries.h"
#include "tools.h"

#define KEY_TAB		9
#define KEY_RETURN	10
#define KEY_ESCAPE	27
#define KEYS_PAGE	91
#define KEYS_OTHER	126
#define KEY_BACKSPACE	127

#define COMPOSING_TIMEOUT 5

static gboolean	 keylog_active;
static int 	 last_key;

#define IS_ALIVE_SERVER(server)							\
	((server) != NULL							\
	&& g_slist_find(servers, (server)) != NULL				\
	&& (server)->connected)

static gboolean
stop_composing(gpointer *user_data)
{
	XMPP_QUERY_REC *query = XMPP_QUERY(user_data);

	if (query == NULL || query->composing_time == 0
	    || !IS_ALIVE_SERVER(query->server))
		return FALSE;
	/* still composing */
	if ((time(NULL) - query->composing_time) < COMPOSING_TIMEOUT)
		return TRUE;
	signal_emit("xmpp composing stop", 2, query->server, query->name);
	query->composing_time = 0;
	return FALSE;
}

static void
sig_gui_key_pressed(int key)
{
	XMPP_QUERY_REC *query;
	time_t current_time;
	char *str = NULL;

	if (!settings_get_bool("xmpp_send_composing") && keylog_active)
		return;
	query = XMPP_QUERY(active_win->active);
	if (query == NULL || !IS_XMPP_SERVER(query->server))
		return;
	/* ignore command or empty line */
	str = parse_special_string("$L", active_win->active_server,
	    active_win->active, "", NULL, 0);
	if (str != NULL &&
	    (*str == *settings_get_str("cmdchars") || *str == '\0'))
		goto out;
	if (key != KEY_TAB && key != KEY_RETURN && last_key != KEY_ESCAPE
	    && key != KEY_ESCAPE && last_key != KEYS_PAGE && key != KEYS_PAGE
	    && key != KEYS_OTHER && key != KEY_BACKSPACE) {
		current_time = time(NULL);
		/* start composing */
		if (query->composing_time == 0) {
			query->composing_time = current_time;
			g_timeout_add(COMPOSING_TIMEOUT * 1000,
			    (GSourceFunc)stop_composing, query);
			signal_emit("xmpp composing start", 2,
			    query->server, query->name);
		/* still composing */
		} else if ((current_time -  query->composing_time)
		    < (COMPOSING_TIMEOUT - 1))
			 query->composing_time = current_time;
	}

out:
	/* message sent */
	if (key == KEY_RETURN)
		 query->composing_time = 0;
	last_key = key;
	g_free(str);
}

static void
keyloger_enabled(gboolean enable)
{
	if (enable && !keylog_active) {
		signal_add_last("gui key pressed", sig_gui_key_pressed);
		keylog_active = TRUE;
	} else if (!enable && keylog_active) {
		signal_remove("gui key pressed", sig_gui_key_pressed);
		keylog_active = FALSE;
	}
}


static void
sig_window_changed(WINDOW_REC *new_window, WINDOW_REC *old_window)
{
	XMPP_SERVER_REC *server;
	XMPP_QUERY_REC *query;

	if (!settings_get_bool("xmpp_send_composing")
	    || (server = XMPP_SERVER(active_win->active_server)) == NULL) {
		keyloger_enabled(FALSE);
		return;
	}
	query = XMPP_QUERY(active_win->active);
	if (query == NULL || !xmpp_have_resource(query->name))
		keyloger_enabled(FALSE);
	else
		keyloger_enabled(TRUE);
}

static void
sig_query_destroyed(QUERY_REC *query_destroyed)
{
	XMPP_QUERY_REC *query;

	query = XMPP_QUERY(query_destroyed);
	if (query != NULL && query->composing_time != 0
	    && IS_ALIVE_SERVER(query->server))
		signal_emit("xmpp composing stop", 2, query->server,
		    query->name);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	GSList *tmp;
	XMPP_QUERY_REC *query;

	if (!IS_XMPP_SERVER(server))
		return;
	for (tmp = queries; tmp != NULL; tmp = tmp->next) {
		query = XMPP_QUERY(tmp->data);
		if (query == NULL)
			continue;
		if (query->server == server)
			g_source_remove_by_user_data(query);
	}
}

void
fe_composing_init(void)
{
	signal_add_last("window changed", sig_window_changed);
	signal_add("query destroyed", sig_query_destroyed);
	signal_add("server disconnected", sig_disconnected);

	settings_add_bool("xmpp", "xmpp_send_composing", TRUE);
	keylog_active = FALSE;
	last_key = 0;
}

void
fe_composing_deinit(void)
{
	signal_remove("window changed", sig_window_changed);
	signal_remove("query destroyed", sig_query_destroyed);
	signal_remove("server disconnected", sig_disconnected);

	keyloger_enabled(FALSE);
}
