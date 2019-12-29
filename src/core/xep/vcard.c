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
#include <irssi/src/core/queries.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "tools.h"
#include "disco.h"

#define XMLNS_VCARD "vcard-temp"

static void
request_vcard(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "vCard", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_VCARD);
	signal_emit("xmpp send iq", 2, server, lmsg);		
	lm_message_unref(lmsg);
}

/* SYNTAX: VCARD [<jid>|<name>]
 * SYNTAX: WHOIS [<jid>|<name>] */
static void
cmd_vcard(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *cmd_dest, *dest;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 1, &cmd_dest))
		return;
	dest = xmpp_get_dest(cmd_dest, server, item);
	request_vcard(server, dest);
	g_free(dest);
	cmd_params_free(free_arg);
}

static void
vcard_handle(XMPP_SERVER_REC *server, const char *jid, LmMessageNode *node)
{
	LmMessageNode *child, *subchild;
	GHashTable *ht;
	const char *adressing;
	char *value;

	ht = g_hash_table_new_full(g_str_hash, g_str_equal,
	    NULL, g_free);
	child = node->children;
	while (child != NULL) {
		/* ignore avatar */
		if (g_ascii_strcasecmp(child->name, "PHOTO") == 0)
			goto next;
		if (child->value != NULL) {
			value = xmpp_recode_in(child->value);
			g_strstrip(value);
			g_hash_table_insert(ht, child->name, value);
			goto next;
		}
		/* find the adressing type indicator */
		subchild = child->children;
		adressing = NULL;
		while (subchild != NULL && adressing == NULL) {
			if (subchild->value == NULL && (
			    g_ascii_strcasecmp(subchild->name , "HOME") == 0 ||
			    g_ascii_strcasecmp(subchild->name , "WORK") == 0))
				adressing = subchild->name;
			subchild = subchild->next;
		}
		subchild = child->children;
		while (subchild != NULL) {
			if (subchild->value != NULL) {
				value = xmpp_recode_in(subchild->value);
				/* TODO sub... */
				g_free(value);
			}
			subchild = subchild->next;
		}

next:
		child = child->next;
	}
	signal_emit("xmpp vcard", 3, server, jid, ht);
	g_hash_table_destroy(ht);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node;

	if (type != LM_MESSAGE_SUB_TYPE_RESULT)
		return;
	node = lm_find_node(lmsg->node, "vCard", XMLNS, XMLNS_VCARD);
	if (node != NULL)
		vcard_handle(server, from, node);
}

void
vcard_init(void)
{
	disco_add_feature(XMLNS_VCARD);
	command_bind_xmpp("vcard", NULL, (SIGNAL_FUNC)cmd_vcard);
	command_bind_xmpp("whois", NULL, (SIGNAL_FUNC)cmd_vcard);
	signal_add("xmpp recv iq", sig_recv_iq);
}

void
vcard_deinit(void)
{
	command_unbind("vcard", (SIGNAL_FUNC)cmd_vcard);
	command_unbind("whois", (SIGNAL_FUNC)cmd_vcard);
	signal_remove("xmpp recv iq", sig_recv_iq);
}
