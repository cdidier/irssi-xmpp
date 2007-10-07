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

#include "xmpp-channels.h"

static void
sig_get_active_channel(const char **name)
{
	*name = IS_XMPP_CHANNEL(active_win->active) ?
	    ((CHANNEL_REC *)active_win->active)->name : NULL;
}

void
fe_xmpp_windows_init(void)
{
	signal_add("xmpp windows get active channel",
	    (SIGNAL_FUNC)sig_get_active_channel);
}

void
fe_xmpp_windows_deinit(void)
{
	signal_remove("xmpp windows get active channel",
	    (SIGNAL_FUNC)sig_get_active_channel);
}
