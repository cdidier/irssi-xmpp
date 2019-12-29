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
 * XEP-0092: Software Version
 */

#include <sys/utsname.h>
#include <string.h>

#include "module.h"
#include <irssi/src/core/queries.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "disco.h"
#include "tools.h"

#define XMLNS_VERSION "jabber:iq:version"

static void
send_version(XMPP_SERVER_REC *server, const char *dest,
    const char *id)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	struct utsname u;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);
	g_free(recoded);
	if (id != NULL)
		lm_message_node_set_attribute(lmsg->node, "id", id);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_VERSION);
	if (settings_get_bool("xmpp_send_version")) {
		lm_message_node_add_child(node, "name",
		    IRSSI_XMPP_PACKAGE);
		lm_message_node_add_child(node, "version",
		    IRSSI_XMPP_VERSION);
		if (uname(&u) == 0)
			lm_message_node_add_child(node, "os", u.sysname);
	}
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
request_version(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	recoded = xmpp_recode_out(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_VERSION);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

/* SYNTAX: VER [[<jid>[/<resource>]]|[<name]] */
static void
cmd_ver(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *cmd_dest, *dest;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 1, &cmd_dest))
		return;
	dest = xmpp_get_dest(cmd_dest, server, item);
	request_version(server, dest);
	g_free(dest);
	cmd_params_free(free_arg);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node, *child;
	char *name, *version, *os;

	if (type == LM_MESSAGE_SUB_TYPE_RESULT
	    && (node = lm_find_node(lmsg->node,"query", XMLNS,
	    XMLNS_VERSION)) != NULL) {
		name = version = os = NULL;
		for (child = node->children; child != NULL; child = child->next) {
			if (child->value == NULL)
				continue;
			if (name == NULL && strcmp(child->name, "name") == 0)
				g_strstrip(name = xmpp_recode_in(child->value));
			else if (version == NULL
			    && strcmp(child->name, "version") == 0)
				g_strstrip(version = xmpp_recode_in(child->value));	
			else if (os  == NULL && strcmp(child->name, "os") == 0)
				g_strstrip(os = xmpp_recode_in(child->value));
		}
		signal_emit("xmpp version", 5, server, from, name, version, os);
		g_free(name);
		g_free(version);
		g_free(os);
	} else if (type == LM_MESSAGE_SUB_TYPE_GET
	    && (node = lm_find_node(lmsg->node,"query", XMLNS,
	    XMLNS_VERSION)) != NULL)
		send_version(server, from, id);
}

void
version_init(void)
{
	disco_add_feature(XMLNS_VERSION);
	settings_add_bool("xmpp", "xmpp_send_version", TRUE);
	command_bind_xmpp("ver", NULL, (SIGNAL_FUNC)cmd_ver);
	signal_add("xmpp recv iq", sig_recv_iq);
}

void
version_deinit(void)
{
	command_unbind("ver", (SIGNAL_FUNC)cmd_ver);
	signal_remove("xmpp recv iq", sig_recv_iq);
}
