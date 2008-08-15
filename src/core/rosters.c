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

#include <stdlib.h>
#include <string.h>

#include "module.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "tools.h"

#define XMLNS_ROSTER "jabber:iq:roster"

const char *xmpp_presence_show[] = {
	"X",
	"-",
	"xa",
	"dnd",
	"away",
	"+",
	"chat",
	"online",
	NULL
};

const char *xmpp_subscription[] = {
	"remove",
	"none",
	"to",
	"from",
	"both",
	NULL
};

static int
func_find_group(gconstpointer group, gconstpointer name)
{
	char *group_name;

	group_name = ((XMPP_ROSTER_GROUP_REC *)group)->name;
	if (group_name == name)
		return 0;
	if (group_name == NULL || name == NULL)
		return -1;
	return strcmp(group_name, name);
}

static int
func_sort_group(gconstpointer group1, gconstpointer group2)
{
	char *group1_name, *group2_name;

	group1_name = ((XMPP_ROSTER_GROUP_REC *)group1)->name;
	group2_name = ((XMPP_ROSTER_GROUP_REC *)group2)->name;
	if (group1_name == NULL || (group2_name != NULL
	    && strcmp(group2_name,
	    settings_get_str("roster_service_name")) == 0))
		return -1;
	if (group1_name != NULL || (group2_name == NULL
	    && strcmp(group1_name,
	    settings_get_str("roster_service_name")) == 0))
		 return 1;
	return strcmp(group1_name, group2_name);
}

static int
func_sort_resource(gconstpointer resource1, gconstpointer resource2)
{
	return ((XMPP_ROSTER_RESOURCE_REC *)resource2)->priority
		- ((XMPP_ROSTER_RESOURCE_REC *)resource1)->priority;
}

static XMPP_ROSTER_RESOURCE_REC *
create_resource(const char *name)
{
	XMPP_ROSTER_RESOURCE_REC *resource;

	resource = g_new(XMPP_ROSTER_RESOURCE_REC, 1);
	resource->name = g_strdup(name);
	resource->priority = 0;
	resource->show= XMPP_PRESENCE_UNAVAILABLE;
	resource->status = NULL;
	resource->composing_id = NULL;
	return resource;
}

static void
cleanup_resource(gpointer data, gpointer user_data)
{
	XMPP_ROSTER_RESOURCE_REC *resource;
	
	if (data == NULL)
		return;
	resource = (XMPP_ROSTER_RESOURCE_REC *)data;
	g_free(resource->name);
	g_free(resource->status);
	g_free(resource->composing_id);
	g_free(resource);
}

static XMPP_ROSTER_USER_REC *
create_user(const char *jid, const char *name)
{
	XMPP_ROSTER_USER_REC *user;

	g_return_val_if_fail(jid != NULL, NULL);
	user = g_new(XMPP_ROSTER_USER_REC, 1);
	user->jid = g_strdup(jid);
	user->name = g_strdup(name);
	user->subscription = XMPP_SUBSCRIPTION_NONE;
	user->error = FALSE;
	user->resources = NULL;
	return user;
}

static void
cleanup_user(gpointer data, gpointer user_data)
{
	XMPP_ROSTER_USER_REC *user;
   
	if (data == NULL)
		return;
	user = (XMPP_ROSTER_USER_REC *)data;
	g_slist_foreach(user->resources, cleanup_resource, NULL);
	g_slist_free(user->resources);
	g_free(user->name);
	g_free(user->jid);
	g_free(user);
}

static XMPP_ROSTER_GROUP_REC *
create_group(const char *name)
{
	XMPP_ROSTER_GROUP_REC *group;

	group = g_new(XMPP_ROSTER_GROUP_REC, 1);
	group->name = g_strdup(name);
	group->users = NULL;
	return group;
}

static void
cleanup_group(gpointer data, gpointer user_data)
{
	XMPP_ROSTER_GROUP_REC *group;

	if (data == NULL)
		return;
	group = (XMPP_ROSTER_GROUP_REC *)data;
	g_slist_foreach(group->users, cleanup_user, group);
	g_slist_free(group->users);
	g_free(group->name);
	g_free(group);
}

static void
roster_cleanup(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server) || server->roster == NULL)
		return;
	g_slist_foreach(server->roster, cleanup_group, server);
	g_slist_free(server->roster);
	server->roster = NULL;
	g_slist_foreach(server->my_resources, cleanup_resource, NULL);
	g_slist_free(server->my_resources);
	server->my_resources = NULL;
}

static XMPP_ROSTER_GROUP_REC *
find_or_add_group(XMPP_SERVER_REC *server, const char *group_name)
{
	GSList *group_list;
	XMPP_ROSTER_GROUP_REC *group;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	group_list = g_slist_find_custom(server->roster, group_name,
	    func_find_group);
	if (group_list == NULL) {
		group = create_group(group_name);
		server->roster = g_slist_insert_sorted(server->roster, group,
		    func_sort_group);
	} else 
		group = group_list->data;
	return group;
}

static XMPP_ROSTER_USER_REC *
add_user(XMPP_SERVER_REC *server, const char *jid, const char *name,
    const char *group_name, XMPP_ROSTER_GROUP_REC **return_group)
{
	XMPP_ROSTER_GROUP_REC *group;
	XMPP_ROSTER_USER_REC *user;

	g_return_val_if_fail(IS_XMPP_SERVER(server), NULL);
	g_return_val_if_fail(jid != NULL, NULL);
	group = find_or_add_group(server, group_name);
	user = create_user(jid, name);
	group->users = g_slist_append(group->users, user);
	if (return_group != NULL)
		*return_group = group;
	return user;
}

static XMPP_ROSTER_GROUP_REC *
move_user(XMPP_SERVER_REC *server, XMPP_ROSTER_USER_REC *user,
    XMPP_ROSTER_GROUP_REC *group, const char *group_name)
{
	XMPP_ROSTER_GROUP_REC *new_group;

	g_return_val_if_fail(IS_XMPP_SERVER(server), group);
        g_return_val_if_fail(user != NULL, group);
	new_group = find_or_add_group(server, group_name);
	group->users = g_slist_remove(group->users, user);
	new_group->users = g_slist_append(new_group->users, user);
	return new_group;
}

static void
update_subscription(XMPP_SERVER_REC *server, XMPP_ROSTER_USER_REC *user,
    XMPP_ROSTER_GROUP_REC *group, const char *subscription)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(user != NULL);
	g_return_if_fail(group != NULL);
	g_return_if_fail(subscription != NULL);
	if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_NONE]) == 0)
		user->subscription = XMPP_SUBSCRIPTION_NONE;
	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_FROM]) == 0)
		user->subscription = XMPP_SUBSCRIPTION_FROM;
	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_TO]) == 0)
		user->subscription = XMPP_SUBSCRIPTION_TO;
	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_BOTH]) == 0)
		user->subscription = XMPP_SUBSCRIPTION_BOTH;
	else if (g_ascii_strcasecmp(subscription,
	    xmpp_subscription[XMPP_SUBSCRIPTION_REMOVE]) == 0) {
		group->users = g_slist_remove(group->users, user);
		cleanup_user(user, server);
		/* remove empty group */
		if (group->users == NULL) {
			server->roster = g_slist_remove(server->roster, group);
			cleanup_group(group, server);
		}
	}
}

static void
update_user(XMPP_SERVER_REC *server, const char *jid, const char *subscription,
    const char *name, const char *group_name)
{
	XMPP_ROSTER_GROUP_REC *group;
	XMPP_ROSTER_USER_REC *user;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(jid != NULL);
	user = rosters_find_user(server->roster, jid, &group, NULL);
	if (user == NULL)
		user = add_user(server, jid, name, group_name, &group);
	else {
		/* move to another group */
		if ((group->name == NULL && group_name != NULL)
		    || (group->name != NULL && group_name == NULL)
		    || (group->name != NULL && group_name != NULL
		    && strcmp(group->name, group_name) != 0))
			group = move_user(server, user, group, group_name);
		/* change name */
		if ((user->name == NULL && name != NULL)
		    || (user->name != NULL && name == NULL)
		    || (user->name != NULL && name != NULL
		    && strcmp(user->name, name) != 0)) {
			g_free(user->name);
			user->name = g_strdup(name);
		}
	}
	update_subscription(server, user, group, subscription);
}

static void
update_user_presence(XMPP_SERVER_REC *server, const char *full_jid,
    const char *show_str, const char *status, const char *priority_str)
{
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *jid, *res;
	int show, priority;
	gboolean new, own;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);
	new = own = FALSE;
	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	if (user == NULL && !(own = strcmp(jid, server->jid) == 0
	     && strcmp(res, server->resource) != 0))
		goto out;
	else
		user->error = FALSE;
	/* find resource or create it if it doesn't exist */	
	resource = !own ? rosters_find_resource(user, res) :
	    rosters_find_own_resource(server, res);
	if (resource == NULL) {
		resource = create_resource(res);
		new = TRUE;
		if (!own)
			user->resources =
			    g_slist_prepend(user->resources, resource);
		else
			server->my_resources =
			    g_slist_prepend(server->my_resources, resource);
		signal_emit("xmpp presence online", 3, server, jid, res);
	}
	show = xmpp_get_show(show_str);
	priority = (priority_str != NULL) ?
	    atoi(priority_str) : resource->priority;
	if (new || xmpp_presence_changed(show, resource->show, status,
	    resource->status, priority, resource->priority)) {
		resource->show = show;
		resource->status = g_strdup(status);
		if (resource->priority != priority) {
			resource->priority = priority;
			/* the resource has changed, sort the resources */
			if (!own)
				user->resources = g_slist_sort(
				    user->resources, func_sort_resource);
			else
				server->my_resources = g_slist_sort(
				    server->my_resources, func_sort_resource);
		}
		signal_emit("xmpp presence changed", 4, server, full_jid,
		    resource->show, resource->status);
	}

out:
	g_free(jid);
	g_free(res);
}

static void
user_unavailable(XMPP_SERVER_REC *server, const char *full_jid,
    const char *status)
{
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *jid, *res;
	gboolean own;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);
	own = FALSE;
	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	if (user == NULL && !(own = strcmp(jid, server->jid) == 0))
		goto out;
	resource = !own ? rosters_find_resource(user, res) :
	    rosters_find_own_resource(server, res);
	if (resource == NULL)
		goto out;
	signal_emit("xmpp presence offline", 3, server, jid, res);
	signal_emit("xmpp presence changed", 4, server, full_jid,
	    XMPP_PRESENCE_UNAVAILABLE, status);
	if (!own)
		user->resources = g_slist_remove(user->resources, resource);
	else
		server->my_resources = g_slist_remove(server->my_resources,
		    resource);
	cleanup_resource(resource, NULL);

out:
	g_free(jid);
	g_free(res);
}

static void
user_presence_error(XMPP_SERVER_REC *server, const char *full_jid)
{
	XMPP_ROSTER_USER_REC *user;
	char *jid;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);
	jid = xmpp_strip_resource(full_jid);
	user = rosters_find_user(server->roster, jid, NULL, NULL);
	if (user != NULL) {
		user->error = TRUE;
		signal_emit("xmpp presence changed", 4, server, full_jid,
		    XMPP_PRESENCE_ERROR, NULL);
	}
	g_free(jid);
}


static void
sig_recv_presence(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node, *node_show, *node_priority;
	char *status;

	switch(type) {
	case LM_MESSAGE_SUB_TYPE_AVAILABLE:
		node_show = lm_message_node_get_child(lmsg->node, "show");
		node = lm_message_node_get_child(lmsg->node, "status");
		status = node != NULL ? xmpp_recode_in(node->value) : NULL;
		node_priority = lm_message_node_get_child(lmsg->node, "priority");
		update_user_presence(server, from,
		    node_show != NULL ? node_show->value : NULL, status,
		    node_priority != NULL ? node_priority->value : NULL);
		g_free(status);
		break;
	case LM_MESSAGE_SUB_TYPE_UNAVAILABLE:
		node = lm_message_node_get_child(lmsg->node, "status");
		status = node != NULL ? xmpp_recode_in(node->value) : NULL;
		user_unavailable(server, from, status);
		g_free(status);
		break;
	case LM_MESSAGE_SUB_TYPE_ERROR:
		user_presence_error(server, from);
		break;
	}
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	LmMessageNode *node, *item, *group_node;
	char *jid, *name, *group;
	const char *subscription;

	if (type != LM_MESSAGE_SUB_TYPE_RESULT
	    && type != LM_MESSAGE_SUB_TYPE_SET)
		return;
	node = lm_find_node(lmsg->node, "query", "xmlns", XMLNS_ROSTER);
	if (node == NULL)
		return;
	for (item = node->children; item != NULL; item = item->next) {
		if (strcmp(item->name, "item") != 0)
			continue;
		jid = xmpp_recode_in(lm_message_node_get_attribute(item, "jid"));
		name = xmpp_recode_in(lm_message_node_get_attribute(item, "name"));
		group_node = lm_message_node_get_child(item, "group");
		group = group_node != NULL ?
		    xmpp_recode_in(group_node->value) : NULL;
		subscription = lm_message_node_get_attribute(item,
		    "subscription");
		update_user(server, jid, subscription, name, group);
		g_free(jid);
		g_free(name);
		g_free(group);
	}
}

static void
sig_connected(XMPP_SERVER_REC *server)
{
	LmMessage *lmsg;
	LmMessageNode *node;

	signal_emit("xmpp server status", 2, server, "Requesting the roster.");
	lmsg = lm_message_new_with_sub_type(NULL, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_GET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, "xmlns", "jabber:iq:roster");
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

void
rosters_init(void)
{
	signal_add("server connected", sig_connected);
	signal_add_first("server disconnected", roster_cleanup);
	signal_add("xmpp recv presence", sig_recv_presence);
	signal_add("xmpp recv iq", sig_recv_iq);
}

void
rosters_deinit(void)
{
	signal_remove("server connected", sig_connected);
	signal_remove("server disconnected", roster_cleanup);
	signal_remove("xmpp recv presence", sig_recv_presence);
	signal_remove("xmpp recv iq", sig_recv_iq);
}
