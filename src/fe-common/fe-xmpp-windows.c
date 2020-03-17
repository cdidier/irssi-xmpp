/*
 * Copyright (C) 2007,2008,2009 Colin DIDIER
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
#include <irssi/src/core/channels.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/core/window-items.h>

#include "xmpp-servers.h"

/*
 * Hack to get the name of the current XMPP channel which is necessary
 * to open a query with a nick in the channel
 * (because its jid is like: channel@host/nick)
 */

static void
sig_get_active_channel(const char **name)
{
	*name = IS_XMPP_SERVER(active_win->active_server)
	    && IS_CHANNEL(active_win->active) ?
	    ((CHANNEL_REC *)active_win->active)->name : NULL;
}

void
fe_xmpp_windows_init(void)
{
	signal_add("xmpp windows get active channel", sig_get_active_channel);
}

void
fe_xmpp_windows_deinit(void)
{
	signal_remove("xmpp windows get active channel",
	    sig_get_active_channel);
}
