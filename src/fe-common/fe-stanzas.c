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
#include <irssi/src/core/levels.h>
#include "module-formats.h"
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/core/window-items.h>

#include "xmpp-servers.h"

static WINDOW_REC *
get_console(XMPP_SERVER_REC *server)
{
	WINDOW_REC *window;
	char *name;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	name = g_strconcat("(raw:", (server->connrec->chatnet == NULL ||
	    *server->connrec->chatnet == '\0') ? server->jid :
	    server->connrec->chatnet, ")", (void *)NULL);
	if ((window = window_find_name(name)) == NULL) {
		window = window_create(NULL, TRUE);
		window_set_name(window, name);
		window_change_server(window, server);
	}
	g_free(name);
	return window;
}

static void
sig_xml_in(XMPP_SERVER_REC *server, const char *msg)
{
	WINDOW_REC *window;
	char *len;

	if (!settings_get_bool("xmpp_xml_console"))
		return;
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(msg != NULL);
	if ((window = get_console(server)) != NULL) {
		len = g_strdup_printf("%lu", (unsigned long)strlen(msg));
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_RAW_IN_HEADER, len);
		g_free(len);
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_RAW_MESSAGE, msg);
	}
}

static void
sig_xml_out(XMPP_SERVER_REC *server, const char *msg)
{
	WINDOW_REC *window;
	char *len;

	if (!settings_get_bool("xmpp_xml_console"))
		return;
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(msg != NULL);
	if ((window = get_console(server)) != NULL) {
		len = g_strdup_printf("%lu", (unsigned long)strlen(msg));
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_RAW_OUT_HEADER, len);
		g_free(len);
		printformat_module_window(MODULE_NAME, window, MSGLEVEL_CRAP,
		    XMPPTXT_RAW_MESSAGE, msg);
	}
}

void
fe_stanzas_init(void)
{
	signal_add("xmpp xml in", (SIGNAL_FUNC)sig_xml_in);
	signal_add("xmpp xml out", (SIGNAL_FUNC)sig_xml_out);

	settings_add_bool("xmpp_lookandfeel", "xmpp_xml_console", FALSE);
}

void
fe_stanzas_deinit(void)
{
	signal_remove("xmpp xml in", (SIGNAL_FUNC)sig_xml_in);
	signal_remove("xmpp xml out", (SIGNAL_FUNC)sig_xml_out);
}
