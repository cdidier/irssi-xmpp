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

/*
 * XEP-0066: Out of Band Data
 */

#include "module.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "tools.h"
#include "disco.h"

#define XMLNS_OOB_X	"jabber:x:oob"
#define XMLNS_OOB_IQ	"jabber:iq:oob"

static void
sig_recv_x(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node, *child;
	const char *url, *desc;
	char *url_recoded, *desc_recoded, *str;

	node = lm_find_node(lmsg->node, "x", "xmlns", XMLNS_OOB_X);
	if (node != NULL) {
		child = lm_message_node_get_child(node, "url");
		if (child == NULL || child->value == NULL)
			return;
		url = child->value;
		child = lm_message_node_get_child(node, "desc");
		desc = child != NULL ? child->value : NULL;
		url_recoded = xmpp_recode_in(url);
		if (desc != NULL) {
			desc_recoded = xmpp_recode_in(desc);
			str = g_strconcat(desc_recoded, ": ", url_recoded);
			g_free(url_recoded);
			g_free(desc_recoded);
		} else
			str = url_recoded;
		signal_emit("message private", 4, server, str, from, from);
		g_free(str);
	}
}

void
oob_init(void)
{
	disco_add_feature(XMLNS_OOB_X);
	signal_add("xmpp recv message", sig_recv_x);
	signal_add("xmpp recv presence", sig_recv_x);
}

void
oob_deinit(void)
{
	signal_remove("xmpp recv message", sig_recv_x);
	signal_remove("xmpp recv presence", sig_recv_x);
}
