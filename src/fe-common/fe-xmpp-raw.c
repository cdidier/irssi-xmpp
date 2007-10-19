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
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "signals.h"
#include "window-items.h"

#include "xmpp-servers.h"

static WINDOW_REC *
get_raw_window(XMPP_SERVER_REC *server)
{
	WINDOW_REC *window;
	char *name;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);

	name = g_strconcat("(raw:", (server->connrec->chatnet == NULL ||
	    *server->connrec->chatnet == '\0') ? server->jid :
	    server->connrec->chatnet, ")", NULL);

	window = window_find_name(name);
	if (window == NULL) {
		window = window_create(NULL, TRUE);
		window_set_name(window, name);
		window_change_server(window, server);
	}
	g_free(name);

	return window;
}

static void
sig_raw_in(XMPP_SERVER_REC *server, const char *msg)
{
	WINDOW_REC *window;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(msg != NULL);

	window = get_raw_window(server);
	if (window != NULL) {
		printtext_window(window, MSGLEVEL_CRAP, "RECV[%d]:", strlen(msg));
		printtext_window(window, MSGLEVEL_CRAP, msg);
	}
}

static void
sig_raw_out(XMPP_SERVER_REC *server, const char *msg)
{
	WINDOW_REC *window;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(msg != NULL);

	window = get_raw_window(server);
	if (window != NULL) {
		printtext_window(window, MSGLEVEL_CRAP, "SEND[%d]:", strlen(msg));
		printtext_window(window, MSGLEVEL_CRAP, msg);
	}
}

void
fe_xmpp_raw_init(void)
{
	signal_add("xmpp raw in", (SIGNAL_FUNC)sig_raw_in);
	signal_add("xmpp raw out", (SIGNAL_FUNC)sig_raw_out);
}

void
fe_xmpp_raw_deinit(void)
{
	signal_remove("xmpp raw in", (SIGNAL_FUNC)sig_raw_in);
	signal_remove("xmpp raw out", (SIGNAL_FUNC)sig_raw_out);
}
