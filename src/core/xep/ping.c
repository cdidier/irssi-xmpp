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
 * XEP-0199: XMPP Ping
 * and lagmeter
 */

#include <string.h>
#include <time.h>

#include "module.h"
#include <irssi/src/core/misc.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "tool_datalist.h"
#include "disco.h"
#include "tools.h"

#define XMLNS_PING "urn:xmpp:ping"

#if defined(IRSSI_ABI_VERSION) && IRSSI_ABI_VERSION >= 25
#define NO_TIMEVAL
#define TimeType gint64
#else
#define TimeType GTimeVal
#endif

struct ping_data {
	char    *id;
	TimeType time;
};

static int	 timeout_tag;
static GSList	*supported_servers;
static DATALIST *pings;

#ifdef NO_TIMEVAL

#define set_current_time(var)   (var) = g_get_real_time()
#define clear_time(var)         (var) = 0
#define has_time(var)           ((var) != 0)
#define get_time_sec(var)       ((var) / G_TIME_SPAN_SECOND)
#define get_time_diff(to, from) (to) - (from)

#else

#define set_current_time(var)   g_get_current_time(&(var))
#define clear_time(var)         memset(&(var), 0, sizeof((var)))
#define has_time(var)           ((var).tv_sec != 0)
#define get_time_sec(var)       ((var).tv_sec)
#define get_time_diff(to, from) (int) get_timeval_diff(&(to), &(from))

#endif

static void
request_ping(XMPP_SERVER_REC *server, const char *dest)
{
	struct ping_data *pd;
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	recoded = xmpp_recode_in(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_GET);
	g_free(recoded);
	node = lm_message_node_add_child(lmsg->node, "ping", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_PING);
	if (strcmp(dest, server->domain) == 0) {
		g_free(server->ping_id);
		server->ping_id =
		    g_strdup(lm_message_node_get_attribute(lmsg->node, "id"));
		set_current_time(server->lag_sent);
		server->lag_last_check = time(NULL);
	} else {
		pd = g_new0(struct ping_data, 1);
		pd->id =
		    g_strdup(lm_message_node_get_attribute(lmsg->node, "id"));
		set_current_time(pd->time);
		datalist_add(pings, server, dest, pd);
	}
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
send_ping(XMPP_SERVER_REC *server, const char *dest, const char *id)
{
	LmMessage *lmsg;
	char *recoded;

	recoded = xmpp_recode_in(dest);
	lmsg = lm_message_new_with_sub_type(recoded,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_RESULT);
	g_free(recoded);
	if (id != NULL)
		lm_message_node_set_attribute(lmsg->node, "id", id);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
}

static void
sig_recv_iq(XMPP_SERVER_REC *server, LmMessage *lmsg, const int type,
    const char *id, const char *from, const char *to)
{
	DATALIST_REC *rec;
	LmMessageNode *node;
	TimeType now;
	struct ping_data *pd;

	if (type == LM_MESSAGE_SUB_TYPE_RESULT) {
		/* pong response from server of our ping */
		if (server->ping_id != NULL
		    && (*from == '\0' || strcmp(from, server->domain) == 0)
	    	    && strcmp(id, server->ping_id) == 0) {
			set_current_time(now);
			server->lag = get_time_diff(now, server->lag_sent);
			clear_time(server->lag_sent);
			g_free_and_null(server->ping_id);
			signal_emit("server lag", 1, server);
		} else if (lmsg->node->children == NULL
		    && (rec = datalist_find(pings, server, from)) != NULL) {
			pd = rec->data;
			if (strcmp(id, pd->id) == 0) {
				set_current_time(now);
				signal_emit("xmpp ping", 3, server, from,
				    get_time_diff(now, pd->time));
			}
		}
	} else if (type == LM_MESSAGE_SUB_TYPE_GET) {
		node = lm_find_node(lmsg->node, "ping", XMLNS, XMLNS_PING);
		if (node == NULL)
			node = lm_find_node(lmsg->node, "query", XMLNS,
			    XMLNS_PING);
		if (node != NULL)
			send_ping(server, from,
			    lm_message_node_get_attribute(lmsg->node, "id"));
	}
}

static void
sig_server_features(XMPP_SERVER_REC *server)
{
	if (disco_have_feature(server->server_features, XMLNS_PING)) {
		if (g_slist_find(supported_servers, server) == NULL) {
			supported_servers = g_slist_prepend(supported_servers, server);
		}
	}
}

static void
sig_disconnected(XMPP_SERVER_REC *server)
{
	if (!IS_XMPP_SERVER(server))
		return;
	supported_servers = g_slist_remove(supported_servers, server);
	datalist_cleanup(pings, server);
}

static int
check_ping_func(void)
{
	GSList *tmp;
	XMPP_SERVER_REC *server;
	time_t now;
	int lag_check_time, max_lag;

	lag_check_time = settings_get_time("lag_check_time")/1000;
	max_lag = settings_get_time("lag_max_before_disconnect")/1000;
	if (lag_check_time <= 0)
		return 1;
	now = time(NULL);
	for (tmp = supported_servers; tmp != NULL; tmp = tmp->next) {
		server = XMPP_SERVER(tmp->data);
		if (has_time(server->lag_sent)) {
			/* waiting for lag reply */
			if (max_lag > 1 &&
			    (now - get_time_sec(server->lag_sent)) > max_lag) {
				/* too much lag - disconnect */
				signal_emit("server lag disconnect", 1,
				    server);
				server->connection_lost = TRUE;
				server_disconnect(SERVER(server));
			}
		} else if ((server->lag_last_check + lag_check_time) < now &&
		    server->connected) {
			/* no commands in buffer - get the lag */
			request_ping(server, server->domain);
		}
	}
	return 1;
}

/* SYNTAX: PING [[<jid>[/<resource>]]|[<name]] */
static void
cmd_ping(const char *data, XMPP_SERVER_REC *server, WI_ITEM_REC *item)
{
	char *cmd_dest, *dest;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 1, &cmd_dest))
		return;
	dest = xmpp_get_dest(cmd_dest, server, item);
	request_ping(server, dest);
	g_free(dest);
	cmd_params_free(free_arg);
}

static void
freedata_func(DATALIST_REC *rec)
{
	g_free(((struct ping_data *)rec->data)->id);
	g_free(rec->data);
}

void
ping_init(void)
{
	supported_servers = NULL;
	pings = datalist_new(freedata_func);
	disco_add_feature(XMLNS_PING);
	signal_add("xmpp recv iq", sig_recv_iq);
	signal_add("xmpp server features", sig_server_features);
	signal_add("server disconnected", sig_disconnected);
	command_bind_xmpp("ping", NULL, (SIGNAL_FUNC)cmd_ping);
	timeout_tag = g_timeout_add(1000, (GSourceFunc)check_ping_func, NULL);
}

void
ping_deinit(void)
{
	g_source_remove(timeout_tag);
	signal_remove("xmpp recv iq", sig_recv_iq);
	signal_remove("xmpp server features", sig_server_features);
	signal_remove("server disconnected", sig_disconnected);
	command_unbind("ping", (SIGNAL_FUNC)cmd_ping);
	g_slist_free(supported_servers);
	datalist_destroy(pings);
}
