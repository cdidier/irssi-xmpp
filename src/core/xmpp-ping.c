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

#include <string.h>
#include <time.h>

#include "module.h"
#include "misc.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-protocol.h"
#include "xmpp-servers.h"
#include "xmpp-tools.h"

/*
 * XEP-0199: XMPP Ping
 * and lagmeter
 */

static int timeout_tag;

static void
send_ping(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *msg;
	LmMessageNode *child;
	char *dest_recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));

	dest_recoded = xmpp_recode_in(dest);
	msg = lm_message_new_with_sub_type(dest_recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(dest_recoded);

	child = lm_message_node_add_child(msg->node, "ping", NULL);
	lm_message_node_set_attribute(child, "xmlns", XMLNS_PING);

	if (dest == NULL) {
		g_free(server->ping_id);
		server->ping_id =
		    g_strdup(lm_message_node_get_attribute(msg->node, "id"));

		g_get_current_time(&server->lag_sent);
		server->lag_last_check = time(NULL);
	}

	lm_send(server, msg, NULL);
	lm_message_unref(msg);
}

void
xmpp_ping_send(XMPP_SERVER_REC *server, const char *dest)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(dest != NULL);

	/* TODO save current time */
	send_ping(server, dest);
}

static void
sig_handle_ping(XMPP_SERVER_REC *server, const char *from, const char *id)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(id);

	/* pong response from server of our ping */
	if (server->ping_id != NULL && strcmp(id, server->ping_id) == 0) {
		GTimeVal now;	

		g_get_current_time(&now);
		server->lag = (int)get_timeval_diff(&now, &server->lag_sent);

		memset(&server->lag_sent, 0, sizeof(server->lag_sent));
		g_free_and_null(server->ping_id);	

		signal_emit("server lag", 1, server);
	}
}

static int
check_ping_func(void)
{
	GSList *tmp, *next;
	XMPP_SERVER_REC *server;
	time_t now;
	int lag_check_time, max_lag;

	lag_check_time = settings_get_time("lag_check_time")/1000;
	max_lag = settings_get_time("lag_max_before_disconnect")/1000;

	if (lag_check_time <= 0)
		return 1;

	now = time(NULL);
	for (tmp = servers; tmp != NULL; tmp = next) {
		server = XMPP_SERVER(tmp->data);

		next = tmp->next;
		if (server == NULL ||
		    !(server->features & XMPP_SERVERS_FEATURE_PING))
			continue;

		if (server->lag_sent.tv_sec != 0) {
			/* waiting for lag reply */
			if (max_lag > 1 &&
			    (now - server->lag_sent.tv_sec) > max_lag) {
				/* too much lag - disconnect */
				signal_emit("server lag disconnect", 1,
				    server);
				server->connection_lost = TRUE;
				server_disconnect(SERVER(server));
			}
		} else if ((server->lag_last_check + lag_check_time) < now &&
		    server->connected) {
			/* no commands in buffer - get the lag */
			send_ping(server, NULL);
		}
	}

	return 1;
}

void
xmpp_ping_init(void)
{
	timeout_tag = g_timeout_add(1000, (GSourceFunc)check_ping_func, NULL);
	signal_add("xmpp ping handle", (SIGNAL_FUNC)sig_handle_ping);
}

void
xmpp_ping_deinit(void)
{
	g_source_remove(timeout_tag);
	signal_remove("xmpp ping handle", (SIGNAL_FUNC)sig_handle_ping);
}
