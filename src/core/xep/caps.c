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

 /* XEP-0115:  Entity Capabilities */

#define XMLNS_CAPS "http://jabber.org/protocol/caps"

#include "module.h"
#include "caps.h"
#include "disco.h"
#include "loudmouth/loudmouth.h"

#include <irssi/src/core/signals.h>

static gchar *
ver_hash(void)
{
	GSList *tmp;
	GChecksum *sum;
	GString *str = g_string_new(NULL);
	guint8 *out;
	gsize len;
	gchar *hash;
	g_string_append(str, "client/console//" IRSSI_XMPP_PACKAGE "<");
	for (tmp = disco_my_features(); tmp != NULL; tmp = tmp->next) {
		g_string_append(str, tmp->data);
		g_string_append_c(str, '<');
	}
	sum = g_checksum_new(G_CHECKSUM_SHA1);
	g_checksum_update(sum, str->str, str->len);
	len = g_checksum_type_get_length(G_CHECKSUM_SHA1);
	out = g_new0(guint8, len);
	g_checksum_get_digest(sum, out, &len);
	hash = g_base64_encode(out, len);
	g_free(out);
	g_checksum_free(sum);
	return hash;
}

static void
sig_add_caps(XMPP_SERVER_REC *server, LmMessage *lmsg)
{
	gchar *ver;
	LmMessageNode *c;
	ver = ver_hash();
	c = lm_message_node_add_child(lmsg->node, "c", NULL);
	lm_message_node_set_attribute(c, "xmlns", XMLNS_CAPS);
	lm_message_node_set_attribute(c, "hash", "sha-1");
	lm_message_node_set_attribute(c, "node", "https://github.com/cdidier/irssi-xmpp");
	lm_message_node_set_attribute(c, "ver", ver);
	g_free(ver);
}

void
caps_init(void)
{
	disco_add_feature(XMLNS_CAPS);
	signal_add("xmpp send presence", sig_add_caps);
}

void
caps_deinit(void)
{
	signal_remove("xmpp send presence", sig_add_caps);
}
