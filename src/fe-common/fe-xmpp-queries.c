/*
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

static void
event_raise_query(XMPP_SERVER_REC *server, QUERY_REC *query)
{
    WINDOW_REC *window;
    
    g_return_if_fail(window != NULL);

    window = window_item_window(query);

    if (window != active_win)
        window_set_active(window);

    window_item_set_active(active_win, (WI_ITEM_REC *) query);
}

void
fe_xmpp_queries_init(void)
{
    signal_add("xmpp event raise query", (SIGNAL_FUNC) event_raise_query);
}

void
fe_xmpp_queries_deinit(void)
{   
    signal_remove("xmpp event raise query", (SIGNAL_FUNC) event_raise_query);
}
