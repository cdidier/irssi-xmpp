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

#include <stdlib.h>

#include "module.h"
#include "recode.h"
#include "settings.h"
#include "signals.h"
#include "window-item-def.h"

#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-commands.h"
#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

const char *xmpp_commands[] = {
	"away",
	"quote",
	"roster",
	"whois",
	NULL
};

const char *xmpp_command_roster[] = {
	"add",
	"remove",
	"name",
	"group",
	"accept",
	"deny",
	"subscribe",
	"unsubscribe",
	NULL
};

static char *
cmd_connect_get_line(const char *data)
{
	GHashTable *optlist;
	char *line, *jid, *password, *server, *port,
	     *username, *jid_server, *resource;
	void *free_arg;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
	    "xmppconnect", &optlist, &jid, &password))
		return NULL;

	if (*jid == '\0' || *password == '\0' || !xmpp_jid_have_address(jid))
		goto err;

	xmpp_jid_extract(jid, &username, &jid_server, &resource);

	server = g_hash_table_lookup(optlist, "server");
	if (server == NULL || *server == '\0')
		server = jid_server;

	port = g_hash_table_lookup(optlist, "port");
	if (port == NULL)
		port = "0";

	line = g_strdup_printf("%s-xmppnet xmpp:%s@%s %s %d %s %s@%s%s%s",
	    (g_hash_table_lookup(optlist, "ssl") != NULL) ? "-ssl " : "",
	    username, jid_server, server, atoi(port), password, username,
	    jid_server, (resource != NULL) ? "/" : "",
	    (resource != NULL) ? resource : "");

	cmd_params_free(free_arg);
	return line;

err:
	cmd_params_free(free_arg);
	return NULL;
}

/* SYNTAX: XMPPCONNECT [-ssl] [-server <server>] [-port <port>]
 *                     <jid>[/<resource>] <password> */
static void
cmd_xmppconnect(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	char *line, *cmd_line;

	line = cmd_connect_get_line(data);
	if (line == NULL)
		return;

	cmd_line = g_strdup_printf("%sCONNECT %s",
	    settings_get_str("cmdchars"), line);
	g_free(line);

	signal_emit("send command", 3, cmd_line, server, item);
	g_free(cmd_line);
}

/* SYNTAX: XMPPSERVER [-ssl] [-server <server>] [-port <port>]
 *                    <jid>[/<resource>] <password> */
static void
cmd_xmppserver(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	char *line, *cmd_line;

	line = cmd_connect_get_line(data);
	if (line == NULL)
		return;

	cmd_line = g_strdup_printf("%sSERVER %s",
	    settings_get_str("cmdchars"), line);
	g_free(line);

	signal_emit("send command", 3, cmd_line, server, item);
	g_free(cmd_line);
}

/* SYNTAX: XMPPREGISTER [-ssl] [-server <server>] [-port <port>]
 *                      <jid> <password> */
static void
cmd_xmppregister(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	/* TODO !!! */
}

/* SYNTAX: XMPPUNREGISTER [-ssl] [-server <server>] [-port <port>]
 *                        <jid> <password> */
static void
cmd_xmppunregister(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	/* TODO !!! */
}

static void
send_away(XMPP_SERVER_REC *server, const char *data)
{
	char **tmp;
	const char *show, *reason;

	if (!IS_XMPP_SERVER(server))
		return;

	tmp = g_strsplit(data, " ", 2);

	if (data[0] == '\0')
		show = NULL;
	else {
		show = tmp[0];
		reason = tmp[1];
	}

again:
	if (show == NULL || show[0] == '\0')
		signal_emit("xmpp own_presence", 4, server,
		    XMPP_PRESENCE_AVAILABLE, NULL, server->priority);

	else if (g_ascii_strcasecmp(show,
	    xmpp_presence_show[XMPP_PRESENCE_CHAT]) == 0)
		signal_emit("xmpp own_presence", 4, server, XMPP_PRESENCE_CHAT,
		    reason, server->priority);

	else if (g_ascii_strcasecmp(show,
	    xmpp_presence_show[XMPP_PRESENCE_DND]) == 0)
		signal_emit("xmpp own_presence", 4, server, XMPP_PRESENCE_DND,
		    reason, server->priority);

	else if (g_ascii_strcasecmp(show,
	    xmpp_presence_show[XMPP_PRESENCE_XA]) == 0)
		signal_emit("xmpp own_presence", 4, server, XMPP_PRESENCE_XA,
		    reason, server->priority);

	else if (g_ascii_strcasecmp(show,
	    xmpp_presence_show[XMPP_PRESENCE_AWAY]) == 0 || reason == data)
		signal_emit("xmpp own_presence", 4, server, XMPP_PRESENCE_AWAY,
		    reason, server->priority);

	else {
		reason = data;
		show = settings_get_str("xmpp_default_away_mode");

		goto again;
	}

	g_strfreev(tmp);
}

/* SYNTAX: AWAY [-one | -all] [away|dnd|xa|chat] [<reason>] */
static void
cmd_away(const char *data, XMPP_SERVER_REC *server)
{
	GHashTable *optlist;
	char *reason;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1 | PARAM_FLAG_OPTIONS |
	    PARAM_FLAG_GETREST, "away", &optlist, &reason))
		return;

	if (g_hash_table_lookup(optlist, "one") != NULL)
		send_away(server, reason);
	else 
		g_slist_foreach(servers, (GFunc)send_away, reason);

	cmd_params_free(free_arg);
}

/* SYNTAX: QUOTE <raw_command> */
static void
cmd_quote(const char *data, XMPP_SERVER_REC *server)
{
	char *recoded;

	CMD_XMPP_SERVER(server);

	if (*data == '\0')
		return;

	recoded = xmpp_recode_out(data);

	if (settings_get_bool("xmpp_raw_window"))
		signal_emit("xmpp raw out", 2, server, recoded);

	lm_connection_send_raw(server->lmconn, recoded, NULL);
	g_free(recoded);
}

static void
cmd_roster_show_users(gpointer data, gpointer user_data)
{
	XMPP_SERVER_REC *server = XMPP_SERVER(user_data);
	XMPP_ROSTER_USER_REC *user = (XMPP_ROSTER_USER_REC *)data;

	if (xmpp_rosters_show_user(user))
		signal_emit("xmpp roster nick", 2, server, user);
}

static void
cmd_roster_show_groups(gpointer data, gpointer user_data)
{
	GSList *user_list;
	XMPP_SERVER_REC *server = XMPP_SERVER(user_data);
	XMPP_ROSTER_GROUP_REC *group = (XMPP_ROSTER_GROUP_REC *)data;
	XMPP_ROSTER_USER_REC *user;
	gboolean group_visible;

	user_list = group->users;
	group_visible = FALSE;
	while (!group_visible && user_list != NULL) {
		user = (XMPP_ROSTER_USER_REC *)user_list->data;
		group_visible = xmpp_rosters_show_user(user);
		user_list = user_list->next;
	}

	if (group_visible) {
		xmpp_rosters_reorder(group);

		signal_emit("xmpp roster group", 2, server, group->name);
		g_slist_foreach(group->users, (GFunc)cmd_roster_show_users,
		    server);
	}
}

/* SYNTAX: ROSTER */
static void
cmd_roster(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	CMD_XMPP_SERVER(server);

	if (*data == '\0') {
		signal_emit("xmpp begin of roster", 1, server);
		g_slist_foreach(server->roster, (GFunc)cmd_roster_show_groups,
		     server);
		signal_emit("xmpp end of roster", 1, server);
	} else
		command_runsub(xmpp_commands[XMPP_COMMAND_ROSTER], data,
		    server, item);
}

/* SYNTAX: ROSTER FULL */
static void
cmd_roster_full(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	gboolean oldvalue;

	CMD_XMPP_SERVER(server);
	
	oldvalue = settings_get_bool("roster_show_offline");

	if (!oldvalue)
		settings_set_bool("roster_show_offline", TRUE);

	signal_emit("command roster", 3, "", server, item);

	if (!oldvalue)
		settings_set_bool("roster_show_offline", oldvalue);
}

/* SYNTAX: ROSTER ADD <jid> */
static void
cmd_roster_add(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *query_node, *item_node;
	char *jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;

	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;
	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(NULL , LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:roster");

	item_node = lm_message_node_add_child(query_node, "item", NULL);
	lm_message_node_set_attribute(item_node, "jid", jid_recoded);

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	if (settings_get_bool("roster_add_send_subscribe")) {
		msg = lm_message_new_with_sub_type(jid_recoded,
		    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_SUBSCRIBE);
		lm_send(server, msg, NULL);
		lm_message_unref(msg);
	}

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}


/* SYNTAX: ROSTER REMOVE <jid> */
static void
cmd_roster_remove(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *query_node, *item_node;
	XMPP_ROSTER_USER_REC *user;
	char *jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL) {
		signal_emit("xmpp not in roster", 2, server, jid);
		goto out;
	}

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(NULL , LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_SET);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:roster");

	item_node = lm_message_node_add_child(query_node, "item", NULL);
	lm_message_node_set_attribute(item_node, "jid", jid_recoded);
	lm_message_node_set_attribute(item_node, "subscription", "remove");

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER NAME <jid> [<name>] */
static void
cmd_roster_name(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *query_node, *item_node;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_GROUP_REC *group;
	char *jid, *jid_recoded, *name, *name_recoded, *group_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &jid,
	    &name))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	user = xmpp_rosters_find_user(server->roster, jid, &group);
	if (user == NULL) {
		signal_emit("xmpp not in roster", 2, server, jid);
		goto out;
	}

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(NULL , LM_MESSAGE_TYPE_IQ,
	     LM_MESSAGE_SUB_TYPE_SET);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:roster");

	item_node = lm_message_node_add_child(query_node, "item", NULL);
	lm_message_node_set_attribute(item_node, "jid", jid_recoded);

	if (group->name != NULL) {
		group_recoded = xmpp_recode_out(group->name);
		lm_message_node_add_child(item_node, "group", group_recoded);
		g_free(group_recoded);
	}

	if (*name != '\0') {
		name_recoded = xmpp_recode_out(name);
		lm_message_node_set_attribute(item_node, "name", name_recoded);
		g_free(name_recoded);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER GROUP <jid> [<group>] */
static void
cmd_roster_group(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *query_node, *item_node;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_GROUP_REC *group;
	char *jid, *jid_recoded, *name_recoded, *group_name, *group_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &jid,
	    &group_name))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	user = xmpp_rosters_find_user(server->roster, jid, &group);
	if (user == NULL) {
		signal_emit("xmpp not in roster", 2, server, jid);
		goto out;
	}

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(NULL , LM_MESSAGE_TYPE_IQ,
	     LM_MESSAGE_SUB_TYPE_SET);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, "xmlns", "jabber:iq:roster");

	item_node = lm_message_node_add_child(query_node, "item", NULL);
	lm_message_node_set_attribute(item_node, "jid", jid_recoded);

	if (*group_name != '\0') {
		group_recoded = xmpp_recode_out(group_name);
		lm_message_node_add_child(item_node, "group", group_recoded);
		g_free(group_recoded);
	}

	if (user->name != NULL) {
		name_recoded = xmpp_recode_out(user->name);
		lm_message_node_set_attribute(item_node, "name", name_recoded);
		g_free(name_recoded);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER ACCEPT <jid> */
static void
cmd_roster_accept(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	char *jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(jid_recoded,
	    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_SUBSCRIBED);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER DENY <jid> */
static void
cmd_roster_deny(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	char *jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(jid_recoded,
	    LM_MESSAGE_TYPE_PRESENCE,LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER SUBSCRIBE <jid> [<reason>] */
static void
cmd_roster_subscribe(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	char *jid, *jid_recoded, *reason, *reason_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST, &jid,
	    &reason))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	jid_recoded = xmpp_recode_out(jid);

	msg = lm_message_new_with_sub_type(jid_recoded,
	    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_SUBSCRIBE);

	if (*reason != '\0') {
		reason_recoded = xmpp_recode_out(reason);
		lm_message_node_add_child(msg->node, "status", reason_recoded);
		g_free(reason_recoded);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ROSTER UNSUBSCRIBE <jid> */
static void
cmd_roster_unsubscribe(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	char *jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;
	
	if (*jid == '\0' || !xmpp_jid_have_address(jid))
		goto out;

	jid_recoded = xmpp_recode_out(jid);


	msg = lm_message_new_with_sub_type(jid_recoded,
	    LM_MESSAGE_TYPE_PRESENCE, LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE);
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: WHOIS [<jid>|<name>] */
static void
cmd_whois(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *node;
	char *jid, *full_jid, *jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;

	if (*jid == '\0')
		jid_recoded = xmpp_recode_out(server->jid);

	else if (xmpp_jid_have_address(jid))
		jid_recoded = xmpp_recode_out(jid);

	else {
		full_jid = xmpp_rosters_get_full_jid(server->roster, jid);
		if (full_jid == NULL)
			goto out;
	
		jid_recoded = xmpp_recode_out(full_jid);
		g_free(full_jid);
	}

	/* request the vcard */
	msg = lm_message_new_with_sub_type(jid_recoded, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child(msg->node, "vCard", NULL);
	lm_message_node_set_attribute(node, "xmlns", "vcard-temp");

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: VER [[<jid>[/<resource>]]|[<name]] */
static void
cmd_ver(const char *data, XMPP_SERVER_REC *server)
{
	LmMessage *msg;
	LmMessageNode *node;
	char *jid, *full_jid, *full_jid_recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 1, &jid))
		return;

	if (*jid == '\0') {
		full_jid = g_strconcat(server->jid, "/",
		    server->resource, NULL);
		full_jid_recoded = xmpp_recode_out(full_jid);
		g_free(full_jid);

	} else if (xmpp_jid_have_address(jid) && xmpp_jid_have_resource(jid))
		full_jid_recoded = xmpp_recode_out(jid);

	else {
		full_jid = xmpp_rosters_get_full_jid(server->roster, jid);
		if (full_jid == NULL || !xmpp_jid_have_resource(full_jid)) {
			g_free(full_jid);
			goto out;
		}

		full_jid_recoded = xmpp_recode_out(full_jid);
		g_free(full_jid);
	}

	/* request the software version */
	msg = lm_message_new_with_sub_type(full_jid_recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", "jabber:iq:version");

	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(full_jid_recoded);

out:
	cmd_params_free(free_arg);
}

/* SYNTAX: ME <message> */
static void
cmd_me(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	const char *target;
	char *text, *recoded;
	int type;

	CMD_XMPP_SERVER(server);

	if (!IS_XMPP_ITEM(item) || !xmpp_server_is_alive(server))
		return;

	target = window_item_get_target(item);
	type = IS_CHANNEL(item) ? SEND_TARGET_CHANNEL : SEND_TARGET_NICK;

	signal_emit("message xmpp own_action", 3, server, data, target,
	    GINT_TO_POINTER(type));

	text = g_strconcat(settings_get_str("cmdchars"), "me ", data, NULL);

	recoded = recode_out(SERVER(server), text, target);
	server->send_message(SERVER(server), target, recoded, type);
	g_free(recoded);

	g_free(text);
}

/* SYNTAX: PART [<channels>] [<message>] */
static void
cmd_part(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channame, *msg;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST |
	    PARAM_FLAG_OPTCHAN, item, &channame, &msg))
		return;

	if (*channame == '\0') {
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	}

	if (*msg == '\0')
		 msg = (char *)settings_get_str("part_message");

	signal_emit("xmpp channels part", 3, server, channame, msg);

	cmd_params_free(free_arg);
}

/* SYNTAX: NICK [<channels>] <nick> */
static void
cmd_nick(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *channame, *nick;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_GETREST |
	    PARAM_FLAG_OPTCHAN, item, &channame, &nick))
		return;

	if (*channame == '\0' || *nick == '\0') {
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	}

	signal_emit("xmpp channels own_nick", 3, server, channame, nick);

	cmd_params_free(free_arg);
}

/* SYNTAX: TOPIC [-delete] [<channel>] [<topic>] */
static void
cmd_topic(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	LmMessage *msg;
	char *channame, *topic, *recoded;
	void *free_arg;

	CMD_XMPP_SERVER(server);

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTCHAN |
	    PARAM_FLAG_OPTIONS | PARAM_FLAG_GETREST, item, "topic", &optlist,
	    &channame, &topic))
		return;

	if (*topic != '\0' || g_hash_table_lookup(optlist, "delete") != NULL) {
		msg = lm_message_new_with_sub_type(channame,
		    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_GROUPCHAT);

		if (g_hash_table_lookup(optlist, "delete") != NULL)
			lm_message_node_add_child(msg->node, "subject", NULL);
		else {
			recoded = xmpp_recode_in(topic);
			lm_message_node_add_child(msg->node, "subject",
			    recoded);
			g_free(recoded);
		}

		lm_send(server, msg, NULL);
		lm_message_unref(msg);
	}

	cmd_params_free(free_arg);
}

void
xmpp_commands_init(void)
{
	command_bind("xmppconnect", NULL, (SIGNAL_FUNC)cmd_xmppconnect);
	command_bind("xmppserver", NULL, (SIGNAL_FUNC)cmd_xmppserver);
	command_bind("xmppregister", NULL, (SIGNAL_FUNC)cmd_xmppregister);
	command_bind("xmppunregister", NULL, (SIGNAL_FUNC)cmd_xmppunregister);
	command_bind_xmpp("away", NULL, (SIGNAL_FUNC)cmd_away);
	command_bind_xmpp("quote", NULL, (SIGNAL_FUNC)cmd_quote);
	command_bind_xmpp("roster", NULL, (SIGNAL_FUNC)cmd_roster);
	command_bind_xmpp("roster full", NULL, (SIGNAL_FUNC)cmd_roster_full);
	command_bind_xmpp("roster add", NULL, (SIGNAL_FUNC)cmd_roster_add);
	command_bind_xmpp("roster remove", NULL,
	    (SIGNAL_FUNC)cmd_roster_remove);
	command_bind_xmpp("roster name", NULL, (SIGNAL_FUNC)cmd_roster_name);
	command_bind_xmpp("roster group", NULL, (SIGNAL_FUNC)cmd_roster_group);
	command_bind_xmpp("roster accept", NULL,
	    (SIGNAL_FUNC)cmd_roster_accept);
	command_bind_xmpp("roster deny", NULL,
	    (SIGNAL_FUNC)cmd_roster_deny);
	command_bind_xmpp("roster subscribe", NULL,
	    (SIGNAL_FUNC)cmd_roster_subscribe);
	command_bind_xmpp("roster unsubscribe", NULL,
	    (SIGNAL_FUNC)cmd_roster_unsubscribe);
	command_bind_xmpp("whois", NULL, (SIGNAL_FUNC)cmd_whois);
	command_bind_xmpp("ver", NULL, (SIGNAL_FUNC)cmd_ver);
	command_bind_xmpp("me", NULL, (SIGNAL_FUNC)cmd_me);
	command_bind_xmpp("part", NULL, (SIGNAL_FUNC)cmd_part);
	command_bind_xmpp("nick", NULL, (SIGNAL_FUNC)cmd_nick);
	command_bind_xmpp("topic", NULL, (SIGNAL_FUNC)cmd_topic);

	command_set_options("connect", "+xmppnet");
	command_set_options("server add", "-xmppnet");
	command_set_options("xmppconnect", "ssl -server @port @priority");

	settings_add_str("xmpp", "xmpp_default_away_mode", "away");
	settings_add_bool("xmpp", "roster_add_send_subscribe", TRUE);
}

void
xmpp_commands_deinit(void)
{
	command_unbind("xmppconnect", (SIGNAL_FUNC)cmd_xmppconnect);
	command_unbind("xmppserver", (SIGNAL_FUNC)cmd_xmppserver);
	command_unbind("xmppregister", (SIGNAL_FUNC)cmd_xmppregister);
	command_unbind("xmppunregister", (SIGNAL_FUNC)cmd_xmppunregister);
	command_unbind("away", (SIGNAL_FUNC)cmd_away);
	command_unbind("quote", (SIGNAL_FUNC)cmd_quote);
	command_unbind("roster", (SIGNAL_FUNC)cmd_roster);
	command_unbind("roster full", (SIGNAL_FUNC)cmd_roster_full);
	command_unbind("roster add", (SIGNAL_FUNC)cmd_roster_add);
	command_unbind("roster remove", (SIGNAL_FUNC)cmd_roster_remove);
	command_unbind("roster name", (SIGNAL_FUNC)cmd_roster_name);
	command_unbind("roster group", (SIGNAL_FUNC)cmd_roster_group);
	command_unbind("roster accept", (SIGNAL_FUNC)cmd_roster_accept);
	command_unbind("roster deny", (SIGNAL_FUNC)cmd_roster_deny);
	command_unbind("roster subscribe", (SIGNAL_FUNC)cmd_roster_subscribe);
	command_unbind("roster unsubscribe",
	    (SIGNAL_FUNC)cmd_roster_unsubscribe);
	command_unbind("whois", (SIGNAL_FUNC)cmd_whois);
	command_unbind("ver", (SIGNAL_FUNC)cmd_ver);
	command_unbind("me", (SIGNAL_FUNC)cmd_me);
	command_unbind("part", (SIGNAL_FUNC)cmd_part);
	command_unbind("nick", (SIGNAL_FUNC)cmd_nick);
	command_unbind("topic", (SIGNAL_FUNC)cmd_topic);
}
