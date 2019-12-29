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

/*
 * XEP-0085: Chat State Notifications
 */

#include "module.h"
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "disco.h"

#define XMLNS_CHATSTATES "http://jabber.org/protocol/chatstates"

static void
sig_recv_message(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	if ((type != LM_MESSAGE_SUB_TYPE_NOT_SET
	    && type != LM_MESSAGE_SUB_TYPE_HEADLINE
	    && type != LM_MESSAGE_SUB_TYPE_NORMAL
	    && type != LM_MESSAGE_SUB_TYPE_CHAT)
	    || server->ischannel(SERVER(server), from))
		return;
	if (lm_find_node(lmsg->node, "composing", XMLNS,
	    XMLNS_CHATSTATES) != NULL) {
		signal_emit("xmpp composing show", 2, server, from);
	} else if (lm_find_node(lmsg->node, "active", XMLNS,
	    XMLNS_CHATSTATES) != NULL
	    || lm_find_node(lmsg->node, "paused", XMLNS,
	    XMLNS_CHATSTATES) != NULL)
		signal_emit("xmpp composing hide", 2, server, from);
}

void
chatstates_init(void)
{
	disco_add_feature(XMLNS_CHATSTATES);
	signal_add("xmpp recv message", sig_recv_message);
}

void
chatstates_deinit(void)
{
	signal_remove("xmpp recv message", sig_recv_message);
}
