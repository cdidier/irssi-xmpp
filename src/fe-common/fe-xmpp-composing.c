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
#include "module-formats.h"
#include "signals.h"
#include "window-items.h"
#include "levels.h"
#include "printtext.h"
#include "themes.h"
#include "settings.h"

#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-protocol.h"

#define KEY_TAB 9
#define KEY_RETURN 10
#define KEY_ESCAPE 27
#define KEYS_PAGE 91
#define KEYS_OTHER 126
#define KEY_BACKSPACE 127

struct composing_query {
	XMPP_SERVER_REC *server;
	gchar *full_jid;
};

gboolean keylog_active = FALSE;
int last_key = 0;
time_t last_time;
struct composing_query *last_compo;
GSList *compo_list;

static gboolean
stop_composing(gpointer *user_data)
{
	struct composing_query *compo;
	time_t current_time;

	compo = (struct composing_query *)user_data;

	/* still composing */
	current_time = time(NULL);
	if ((compo == last_compo)
	    && ((current_time - last_time) < XMPP_COMPOSING_TIMEOUT))
		return TRUE;

	/* server still connected */
	if (g_slist_find(servers, compo->server) == NULL)
		return FALSE;

	xmpp_send_stop_composing(compo->server, compo->full_jid);

	return FALSE;
}

static void
stop_composing_destroy(gpointer *user_data)
{
	g_free(((struct composing_query *)user_data)->full_jid);
	g_free(user_data);
}

static void
sig_gui_key_pressed(int key)
{
	XMPP_SERVER_REC *server;
	QUERY_REC *query;
	struct composing_query *compo;
	time_t current_time;

	if (key != KEY_TAB && key != KEY_RETURN && last_key != KEY_ESCAPE
	    && key != KEY_ESCAPE && last_key != KEYS_PAGE && key != KEYS_PAGE
	    && key != KEYS_OTHER && key != KEY_BACKSPACE) {

		if (!settings_get_bool("xmpp_send_composing"))
			goto last;

		server = XMPP_SERVER(active_win->active_server);
		if (server == NULL)
			goto last;

		query = XMPP_QUERY(active_win->active);
		if (query == NULL || !xmpp_jid_have_ressource(query->name))
			 goto last;

		/* composing */
		current_time = time(NULL);
		if ((current_time - last_time) < (XMPP_COMPOSING_TIMEOUT - 1))
			goto last;

	//      if (compo != last_compo) {
			compo = g_new(struct composing_query, 1);
			compo->server = server;
			compo->full_jid = g_strdup(query->name);
			g_timeout_add_full(G_PRIORITY_DEFAULT,
			    XMPP_COMPOSING_TIMEOUT * 1000,
			    (GSourceFunc)stop_composing, compo,
			    (GDestroyNotify)stop_composing_destroy);
			last_compo = compo;
	//      }

		last_time = current_time;
		xmpp_send_composing(server, query->name);

	} else if (key == KEY_RETURN)
		last_time = time(NULL) - XMPP_COMPOSING_TIMEOUT - 1;

last:
	last_key = key;
}

static void
sig_window_changed(WINDOW_REC *new_window, WINDOW_REC *old_window)
{
	XMPP_SERVER_REC *server;
	QUERY_REC *query;

	if (!settings_get_bool("xmpp_send_composing"))
		goto stop;

	server = XMPP_SERVER(active_win->active_server);
	if (server == NULL)
		goto stop;

	query = XMPP_QUERY(active_win->active);
	if (query == NULL || !xmpp_jid_have_ressource(query->name))
		goto stop;

	if (!keylog_active) {
		signal_add_last("gui key pressed",
		    (SIGNAL_FUNC)sig_gui_key_pressed);

		keylog_active = TRUE;
		last_time = time(NULL) - XMPP_COMPOSING_TIMEOUT - 1;
	}
	return;

stop:
	if (keylog_active)
		signal_remove("gui key pressed",
		    (SIGNAL_FUNC)sig_gui_key_pressed);
}

void
fe_xmpp_composing_init(void) {
	signal_add_last("window changed", (SIGNAL_FUNC)sig_window_changed);

	compo_list = NULL;
}

fe_xmpp_composing_deinit(void) {
	 signal_remove("window changed", (SIGNAL_FUNC)sig_window_changed);

	g_slist_free(compo_list);
}
