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

#include "module.h"
#include "core.h"
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "servers-setup.h"
#include "settings.h"
#include "signals.h"
#include "themes.h"

#include "fe-xmpp-queries.h"
#include "fe-xmpp-status.h"
#include "fe-rosters.h"
#include "fe-stanzas.h"
#include "xmpp-completion.h"

static void
sig_server_status(SERVER_REC *server, const char *msg)
{
	printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CLIENTNOTICE,
	    XMPPTXT_DEFAULT_EVENT, NULL, msg, NULL);
}

static void
sig_server_add_fill(SERVER_SETUP_REC *rec, GHashTable *optlist)
{
	char *value;

	value = g_hash_table_lookup(optlist, "xmppnet");
	if (value != NULL) {
		g_free_and_null(rec->chatnet);
		if (*value != '\0')
		    rec->chatnet = g_strdup(value);
	}
}

void
fe_xmpp_init(void)
{
	theme_register(fecommon_xmpp_formats);

	signal_add("xmpp server status", (SIGNAL_FUNC)sig_server_status);
	signal_add("server add fill", (SIGNAL_FUNC)sig_server_add_fill);

	fe_xmpp_queries_init();
	fe_xmpp_status_init();
	fe_rosters_init();
	fe_stanzas_init();
	xmpp_completion_init();

	module_register("xmpp", "fe");

	/* load irssi-xmpp's fe-text submodule */
/*	if (irssi_gui == IRSSI_GUI_TEXT) {
		char *cmd_line = g_strconcat(settings_get_str("cmdchars"),
		    "load xmpp text", NULL);
		signal_emit("send command", 1, cmd_line);
		g_free(cmd_line);
	}*/
}

void
fe_xmpp_deinit(void)
{
	signal_remove("xmpp server status", (SIGNAL_FUNC)sig_server_status);
	signal_remove("server add fill", (SIGNAL_FUNC)sig_server_add_fill);

	fe_xmpp_queries_deinit();
	fe_xmpp_status_deinit();
	fe_rosters_deinit();
	fe_stanzas_deinit();
	xmpp_completion_deinit();

	theme_unregister();
}
