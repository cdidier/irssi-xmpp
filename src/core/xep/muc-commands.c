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

#include <stdlib.h>

#include "module.h"
#include "settings.h"
#include "signals.h"
#include "window-item-def.h"

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "tools.h"
#include "rosters-tools.h"
#include "muc.h"

/* SYNTAX: INVITE <jid>[/<resource>]|<name> [<channame>] */
static void
cmd_invite(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	MUC_REC *channel;
	LmMessage *lmsg;
	LmMessageNode *node, *invite_node;
	GHashTable *optlist;
	char *channame, *dest, *jid, *recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
	    "xmppinvite", &optlist, &dest, &channame))
		return;
	if (*dest == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if (*channame == '\0' || g_ascii_strcasecmp(channame, "*") == 0) {
		if (!IS_MUC(item))
			cmd_param_error(CMDERR_NOT_JOINED);
		channame = MUC(item)->name;
	}
	if ((channel = muc_find(server, channame)) == NULL)
		cmd_param_error(CMDERR_NOT_JOINED);
	if ((jid = rosters_resolve_name(server, dest)) != NULL)
		dest = jid;
	recoded = xmpp_recode_out(channame);
	lmsg = lm_message_new(recoded, LM_MESSAGE_TYPE_MESSAGE);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "x", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_MUC_USER);
	invite_node = lm_message_node_add_child(node, "invite", NULL);
	recoded = xmpp_recode_out(dest);
	lm_message_node_set_attribute(invite_node, "to", recoded);
	g_free(recoded);
	signal_emit("xmpp send message", 2, server, lmsg);
	lm_message_unref(lmsg);
	g_free(jid);
	cmd_params_free(free_arg);
}

/* SYNTAX: PART [<channel] [<message>] */
static void
cmd_part(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	MUC_REC *channel;
	char *channame, *reason;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST |
	    PARAM_FLAG_OPTCHAN, item, &channame, &reason))
		return;
	if (*channame == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if ((channel = muc_find(server, channame)) == NULL)
		cmd_param_error(CMDERR_NOT_JOINED);
	if (*reason == '\0')
		reason = (char *)settings_get_str("part_message");
	muc_part(channel, reason);
	cmd_params_free(free_arg);
}

/* SYNTAX: NICK [<channel>] <nick> */
static void
cmd_nick(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	MUC_REC *channel;
	char *channame, *nick;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST |
	    PARAM_FLAG_OPTCHAN, item, &channame, &nick))
		return;
	if (*channame == '\0' || *nick == '\0')
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	if ((channel = muc_find(server, channame)) == NULL)
		cmd_param_error(CMDERR_NOT_JOINED);
	muc_nick(channel, nick);
	cmd_params_free(free_arg);
}

/* SYNTAX: TOPIC [-delete] [<channel>] [<topic>] */
static void
cmd_topic(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	LmMessage *lmsg;
	char *channame, *topic, *recoded;
	void *free_arg;
	gboolean delete;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN |
	    PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST, item, "topic", &optlist,
	    &channame, &topic))
		return;
	if (muc_find(server, channame) == NULL)
		cmd_param_error(CMDERR_NOT_JOINED);
	g_strstrip(topic);
	delete = g_hash_table_lookup(optlist, "delete") != NULL;
	if (*topic != '\0' || delete) {
		recoded = xmpp_recode_out(channame);
		lmsg = lm_message_new_with_sub_type(recoded,
		    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_GROUPCHAT);
		g_free(recoded);
		if (delete)
			lm_message_node_add_child(lmsg->node, "subject", NULL);
		else {
			recoded = xmpp_recode_out(topic);
			lm_message_node_add_child(lmsg->node, "subject",
			    recoded);
			g_free(recoded);
		}
		signal_emit("xmpp send message", 2, server, lmsg);
		lm_message_unref(lmsg);
	}
	cmd_params_free(free_arg);
}

/* SYNTAX: DESTROY [<channel>] [<alternate>] [<reason>]*/
static void
cmd_destroy(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	MUC_REC *channel;
	LmMessage *lmsg;
	char *channame, *reason, *alternate;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 3 | PARAM_FLAG_OPTCHAN |
	    PARAM_FLAG_GETREST, item, &channame, &alternate, &reason))
		return;
	if ((channel = muc_find(server, channame)) == NULL)
		cmd_param_error(CMDERR_NOT_JOINED);
	if (*alternate == '\0')
		alternate = NULL;
	if (*reason == '\0')
		reason = NULL;
	muc_destroy(server, channel, alternate, reason);
	cmd_params_free(free_arg);
}

void
muc_commands_init(void)
{
	command_bind_xmpp("invite", NULL, (SIGNAL_FUNC)cmd_invite);
	command_set_options("invite", "yes");
	command_bind_xmpp("part", NULL, (SIGNAL_FUNC)cmd_part);
	command_bind_xmpp("nick", NULL, (SIGNAL_FUNC)cmd_nick);
	command_bind_xmpp("topic", NULL, (SIGNAL_FUNC)cmd_topic);
	command_bind_xmpp("destroy", NULL, (SIGNAL_FUNC)cmd_destroy);
}

void
muc_commands_deinit(void)
{
	command_unbind("invite", (SIGNAL_FUNC)cmd_invite);
	command_unbind("part", (SIGNAL_FUNC)cmd_part);
	command_unbind("nick", (SIGNAL_FUNC)cmd_nick);
	command_unbind("topic", (SIGNAL_FUNC)cmd_topic);
	command_unbind("destroy", (SIGNAL_FUNC)cmd_destroy);
}
