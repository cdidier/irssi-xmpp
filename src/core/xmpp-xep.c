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

#include <sys/types.h>
#include <sys/utsname.h>
#include <string.h>
#include <time.h>

#include "module.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-protocol.h"
#include "xmpp-servers.h"
#include "xmpp-channels.h"
#include "xmpp-queries.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

/*
 * XEP-0022: Message Events
 */

void
xep_composing_start(XMPP_SERVER_REC *server, const char *full_jid)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *dest_recoded, *jid, *res;
	const char *id;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);

	dest_recoded = xmpp_recode_out(full_jid);
	msg = lm_message_new_with_sub_type(dest_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(dest_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, XMLNS, XMLNS_EVENT);

	lm_message_node_add_child(child, "composing", NULL);

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_rosters_find_resource(user, res);
	if (resource != NULL) {
		id = lm_message_node_get_attribute(msg->node, "id");
		lm_message_node_add_child(child, "id", id);
		g_free_and_null(resource->composing_id);
		resource->composing_id = g_strdup(id);
	}

out:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
	g_free(res);
}

void
xep_composing_stop(XMPP_SERVER_REC *server, const char *full_jid)
{
	LmMessage *msg;
	LmMessageNode *child;
	XMPP_ROSTER_USER_REC *user;
	XMPP_ROSTER_RESOURCE_REC *resource;
	char *full_jid_recoded, *jid, *res;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(full_jid != NULL);

	full_jid_recoded = xmpp_recode_out(full_jid);

	msg = lm_message_new_with_sub_type(full_jid_recoded,
	    LM_MESSAGE_TYPE_MESSAGE, LM_MESSAGE_SUB_TYPE_CHAT);
	g_free(full_jid_recoded);

	child = lm_message_node_add_child(msg->node, "x", NULL);
	lm_message_node_set_attribute(child, XMLNS, XMLNS_EVENT);

	jid = xmpp_strip_resource(full_jid);
	res = xmpp_extract_resource(full_jid);

	if (jid == NULL || res == NULL)
		goto out;
	
	user = xmpp_rosters_find_user(server->roster, jid, NULL);
	if (user == NULL)
		goto out;

	resource = xmpp_rosters_find_resource(user, res);
	if (resource != NULL && resource->composing_id != NULL) {
		lm_message_node_add_child(child, "id", resource->composing_id);
		g_free_and_null(resource->composing_id);
	}

out:
	lm_send(server, msg, NULL);
	lm_message_unref(msg);

	g_free(jid);
	g_free(res);
}


/*
 * XEP-0030: Service Discovery
 */

static int
disco_parse_features(const char *var, XMPP_SERVERS_FEATURES features)
{
	g_return_val_if_fail(var != NULL, 0);

	if (!(features & XMPP_SERVERS_FEATURE_PING) &&
	    g_ascii_strcasecmp(var, XMLNS_PING) == 0)
		return XMPP_SERVERS_FEATURE_PING;
	else
		return 0;
}

void
xep_disco_server(XMPP_SERVER_REC *server, LmMessageNode *query)
{
	LmMessageNode *item;
	const char *var;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(query != NULL);

	server->features = 0;

	item = query->children;
	while(item != NULL) {
		if (g_ascii_strcasecmp(item->name, "feature") != 0)
			goto next;

		var = lm_message_node_get_attribute(item, "var");
		if (var != NULL)
			server->features |= disco_parse_features(var,
			    server->features);

next:
		item = item->next;
	}
}


/*
 * XEP-0092: Software Version
 */

void
xep_version_send(XMPP_SERVER_REC *server, const char *to_jid,
    const char *id)
{
	LmMessage *msg;
	LmMessageNode *query_node;
	struct utsname u;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(to_jid != NULL);

	msg = lm_message_new_with_sub_type(to_jid, LM_MESSAGE_TYPE_IQ,
	    LM_MESSAGE_SUB_TYPE_RESULT);

	if (id != NULL)
		lm_message_node_set_attribute(msg->node, "id", id);

	query_node = lm_message_node_add_child(msg->node, "query", NULL);
	lm_message_node_set_attribute(query_node, XMLNS, XMLNS_VERSION);

	if (settings_get_bool("xmpp_send_version")) {
		lm_message_node_add_child(query_node, "name",
		    IRSSI_XMPP_PACKAGE);
		lm_message_node_add_child(query_node, "version",
		    IRSSI_XMPP_VERSION);

		if (uname(&u) == 0)
			lm_message_node_add_child(query_node, "os", u.sysname);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

void
xep_version_handle(XMPP_SERVER_REC *server, const char *jid,
    LmMessageNode *node)
{
	LmMessageNode *child;
	char *name, *version, *os;

	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(jid != NULL);
	g_return_if_fail(node != NULL);

	name = version = os = NULL;

	for(child = node->children; child != NULL; child = child->next) {
		if (child->value == NULL)
			continue;

		if (name == NULL && strcmp(child->name, "name") == 0)
			name = xmpp_recode_in(child->value);
		else if (version == NULL
		    && strcmp(child->name, "version") == 0)
			version = xmpp_recode_in(child->value);
		else if (os  == NULL && strcmp(child->name, "os") == 0)
			os = xmpp_recode_in(child->value);
	}

	signal_emit("xmpp version", 5, server, jid, name, version, os);

	g_free(name);
	g_free(version);
	g_free(os);
}


/*
 * XEP-0054: vcard-temp
 */

void
xep_vcard_handle(XMPP_SERVER_REC *server, const char *jid,
    LmMessageNode *node)
{
	LmMessageNode *child, *subchild;
	const char *adressing;
	char *value;

	signal_emit("xmpp begin of vcard", 2, server, jid);

	child = node->children;
	while(child != NULL) {

		/* ignore avatar */
		if (g_ascii_strcasecmp(child->name, "PHOTO") == 0)
			goto next;

		if (child->value != NULL) {
			value = xmpp_recode_in(child->value);
			g_strstrip(value);

			signal_emit("xmpp vcard value", 4, server, jid,
			     child->name, value);

			g_free(value);
			goto next;
		}

		/* find the adressing type indicator */
		subchild = child->children;
		adressing = NULL;
		while(subchild != NULL && adressing == NULL) {
			if (subchild->value == NULL && (
			    g_ascii_strcasecmp(subchild->name , "HOME") == 0 ||
			    g_ascii_strcasecmp(subchild->name , "WORK") == 0))
				adressing = subchild->name;

			subchild = subchild->next;
		}

		subchild = child->children;
		while(subchild != NULL) {
			
			if (subchild->value != NULL) {
				value = xmpp_recode_in(subchild->value);
				g_strstrip(value);

				signal_emit("xmpp vcard subvalue", 6, server,
				    jid, child->name, adressing,
				    subchild->name, value);

				g_free(value);
			}

			subchild = subchild->next;
		}

next:
		child = child->next;
	}

	signal_emit("xmpp end of vcard", 2, server, jid);
}
