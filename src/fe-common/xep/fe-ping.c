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

#include "module.h"
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/irc/module-formats.h>

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "../module-formats.h"

static void
sig_ping(XMPP_SERVER_REC *server, const char *jid, long usecs)
{
	printformat_module(IRC_MODULE_NAME, server, jid, MSGLEVEL_CRAP,
	    IRCTXT_CTCP_PING_REPLY, jid, usecs/1000, usecs%1000);
}

void
fe_ping_init(void)
{
	signal_add("xmpp ping", sig_ping);
}

void
fe_ping_deinit(void)
{   
	signal_remove("xmpp ping", sig_ping);
}
