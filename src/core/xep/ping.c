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
 * XEP-0199: XMPP Ping
 * and lagmeter
 */

#include <string.h>
#include <time.h>

#include "module.h"
#include "misc.h"
#include "settings.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "disco.h"
#include "tools.h"

#define XMLNS_PING "urn:xmpp:ping"

static int	 timeout_tag;
static GSList	*supported_servers;

static void
send_ping(XMPP_SERVER_REC *server, const char *dest)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	g_return_if_fail(IS_XMPP_SERVER(server));
	recoded = xmpp_recode_in(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "ping", NULL);
	lm_message_node_set_attribute(node, "xmlns", XMLNS_PING);
	if (strcmp(dest, server->host) == 0) {
		g_free(server->ping_id);
		server->ping_id =
		    g_strdup(lm_message_node_get_attribute(lmsg->node, "id"));
		g_get_current_time(&server->lag_sent);
		server->lag_last_check = time(NULL);
	}
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	g_return_if_fail(IS_XMPP_SERVER(server));
	g_return_if_fail(id);
	GTimeVal now;

	if (type != LM_MESSAGE_SUB_TYPE_RESULT)
		return;
	/* pong response from server of our ping */
	if (server->ping_id != NULL && strcmp(from, server->host) == 0
	    && strcmp(id, server->ping_id) == 0) {
		g_get_current_time(&now);
		server->lag = (int)get_timeval_diff(&now, &server->lag_sent);
		memset(&server->lag_sent, 0, sizeof(server->lag_sent));
		g_free_and_null(server->ping_id);	
		signal_emit("server lag", 1, server);
	}
}

static void
sig_server_features(XMPP_SERVER_REC *server)
{
	if (xmpp_have_feature(server->server_features, XMLNS_PING))
		supported_servers = g_slist_prepend(supported_servers, server);
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	if (IS_XMPP_SERVER(server))
		supported_servers = g_slist_remove(supported_servers, server);
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
	for (tmp = supported_servers; tmp != NULL; tmp = next) {
		server = XMPP_SERVER(tmp->data);
		next = tmp->next;
		if (server == NULL) /* TODO check for supported feature */
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
			send_ping(server, server->host);
		}
	}
	return 1;
}

void
ping_init(void)
{
	supported_servers = NULL;
	xmpp_add_feature(XMLNS_PING);
	signal_add("xmpp recv iq", sig_recv_iq);
	signal_add("xmpp server features", sig_server_features);
	signal_add("server disconnected", sig_disconnected);
	timeout_tag = g_timeout_add(1000, (GSourceFunc)check_ping_func, NULL);
}

void
ping_deinit(void)
{
	g_source_remove(timeout_tag);
	signal_remove("xmpp recv iq", sig_recv_iq);
	signal_remove("xmpp server features", sig_server_features);
	signal_remove("server disconnected", sig_disconnected);
	g_slist_free(supported_servers);
}
