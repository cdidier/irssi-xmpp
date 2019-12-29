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
 * XEP-0077: In-Band Registration
 */

#include <stdlib.h>
#include <string.h>

#include "module.h"
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include "tools.h"

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "loudmouth-tools.h"
#include "disco.h"
#include "registration.h"

#define XMLNS_REGISTRATION "http://jabber.org/features/iq-register"
#define XMLNS_REGISTER "jabber:iq:register"

gboolean set_ssl(LmConnection *, GError **, gpointer, gboolean);
gboolean set_proxy(LmConnection *, GError **);

struct register_data {
	char	*username;
	char	*domain;
	char	*password;
	char	*address;
	int	 port;
	gboolean use_ssl;
	char 	*id;
	LmConnection		*lmconn;
	LmMessageHandler	*handler;
};

GSList *register_data;

static void
rd_cleanup(struct register_data *rd)
{
	register_data = g_slist_remove(register_data, rd);
	g_free(rd->username);
	g_free(rd->domain);
	g_free(rd->password);
	g_free(rd->address);
	g_free(rd->id);
	if (rd->handler != NULL) {
		if (lm_message_handler_is_valid(rd->handler))
			lm_message_handler_invalidate(rd->handler);
		lm_message_handler_unref(rd->handler);
	}
	if (lm_connection_get_state(rd->lmconn) != LM_CONNECTION_STATE_CLOSED)
		lm_connection_close(rd->lmconn, NULL);
	lm_connection_unref(rd->lmconn);
	g_free(rd);
}

static int
registration_error_map(LmMessageNode *node)
{
	if (lm_message_node_get_child(node, "not-authorized") != NULL)
		return REGISTRATION_ERROR_UNAUTHORIZED;
	if (lm_message_node_get_child(node, "registration-required") != NULL)
		return REGISTRATION_ERROR_UNAUTHORIZED_REG;
	if (lm_message_node_get_child(node, "feature-not-implemented") != NULL)
		return REGISTRATION_ERROR_UNIMPLEMENTED;
	if (lm_message_node_get_child(node, "service-unavailable") != NULL)
		return REGISTRATION_ERROR_UNAVAILABLE;
	if (lm_message_node_get_child(node, "conflict") != NULL)
		return REGISTRATION_ERROR_CONFLICT;
	if (lm_message_node_get_child(node, "remote-server-timeout") != NULL)
		return REGISTRATION_ERROR_TIMEOUT;
	return REGISTRATION_ERROR_UNKNOWN;
}

static LmHandlerResult
handle_register(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *lmsg, gpointer user_data)
{
	LmMessageNode *node;
	struct register_data *rd;
	const char *id;
	const char *error;
	char *cmd;
	int code;

	rd = user_data;
	id = lm_message_node_get_attribute(lmsg->node, "id");
	if (id == NULL || (id != NULL && strcmp(id, rd->id) != 0))
		return LM_HANDLER_RESULT_REMOVE_MESSAGE;
	if ((node = lm_message_node_get_child(lmsg->node, "error")) != NULL) {
		error = lm_message_node_get_attribute(node, "code");
		if (error == NULL)
			code = registration_error_map(node);
		else
			code = atoi(error);
		signal_emit("xmpp registration failed", 3, rd->username,
		    rd->domain, GINT_TO_POINTER(code));
	} else {
		signal_emit("xmpp registration succeed", 2, rd->username,
		    rd->domain);
		cmd = g_strdup_printf(
		    "%sXMPPCONNECT %s-host %s -port %d %s@%s %s",
		    settings_get_str("cmdchars"),
		    rd->use_ssl ? "-ssl " : "", rd->address, rd->port,
		    rd->username, rd->domain, rd->password);
		signal_emit("send command", 3, cmd, NULL, NULL);
		g_free(cmd);
	}
	rd_cleanup(rd);
	return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static void
send_register(struct register_data *rd)
{
	LmMessage *lmsg;
	LmMessageNode *node;
	char *recoded;

	lmsg = lm_message_new_with_sub_type(rd->domain,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_REGISTER);
	recoded = xmpp_recode_out(rd->username);
	lm_message_node_add_child(node, "username", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(rd->password);
	lm_message_node_add_child(node, "password", recoded);
	g_free(recoded);
	rd->id = g_strdup(lm_message_node_get_attribute(lmsg->node, "id"));
	if (!lm_connection_send_with_reply(rd->lmconn, lmsg, rd->handler,
	    NULL)) {
		signal_emit("xmpp registration failed", 3, rd->username,
		    rd->domain, REGISTRATION_ERROR_INFO);
		rd_cleanup(rd);
	}
	lm_message_unref(lmsg);
}

static void
register_lm_close_cb(LmConnection *connection, LmDisconnectReason reason,
    gpointer user_data)
{
	struct register_data *rd;

	if (reason == LM_DISCONNECT_REASON_OK)
		return;
	rd = user_data;
	signal_emit("xmpp registration failed", 3, rd->username, rd->domain,
	    REGISTRATION_ERROR_CLOSED);
	rd_cleanup(rd);
}

static void
register_lm_open_cb(LmConnection *connection, gboolean success,
    gpointer user_data)
{
	struct register_data *rd;
	
	rd = user_data;
	if (!success)
		goto err;
	rd->handler = lm_message_handler_new(handle_register, rd, NULL);
	send_register(rd);
	return;

err:
	signal_emit("xmpp registration failed", 3, rd->username, rd->domain,
	    REGISTRATION_ERROR_CONNECTION);
	rd_cleanup(rd);
}

static void
start_registration(struct register_data *rd)
{
	LmConnection  *lmconn;
	GError *error = NULL;

	lmconn = lm_connection_new(NULL);
	if (rd->use_ssl) {
		if (!set_ssl(lmconn, &error, NULL, FALSE))
			goto err;
	} else {
		if (!set_ssl(lmconn, &error, NULL, TRUE))
			goto err;
	}
	if (settings_get_bool("xmpp_use_proxy") && !set_proxy(lmconn, &error))
		goto err;
	if (rd->port <= 0)
		rd->port = rd->use_ssl ? LM_CONNECTION_DEFAULT_PORT_SSL
		    : LM_CONNECTION_DEFAULT_PORT;
	lm_connection_set_server(lmconn, rd->address);
	lm_connection_set_port(lmconn, rd->port);
	lm_connection_set_jid(lmconn, NULL);
	rd->id = NULL;
	rd->lmconn = lmconn;
	rd->handler = NULL;
	register_data = g_slist_prepend(register_data, rd);
	lm_connection_set_disconnect_function(lmconn, register_lm_close_cb,
	    rd, NULL);
	if (!lm_connection_open(lmconn, register_lm_open_cb, rd, NULL,
	    &error))
		goto err;
	return;

err:
	signal_emit("xmpp register error", 3, rd->username, rd->domain,
	    error != NULL ? error->message : NULL);
	if (error != NULL)
		g_error_free(error);
	lm_connection_unref(lmconn);
	rd_cleanup(rd);
}

/* SYNTAX: XMPPREGISTER [-ssl] [-host <server>] [-port <port>]
 *                      <jid> <password> */
static void
cmd_xmppregister(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *str, *jid, *password, *address;
	void *free_arg;
	struct register_data *rd;

	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
	    "xmppconnect", &optlist, &jid, &password))
		return;
	if (*jid == '\0' || *password == '\0' || !xmpp_have_domain(jid))
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	rd = g_new0(struct register_data, 1);
	rd->username = xmpp_extract_user(jid);
	rd->domain = xmpp_extract_domain(jid);
	rd->password = g_strdup(password);
	address = g_hash_table_lookup(optlist, "host");
	if (address == NULL || *address == '\0')
		address = rd->domain;
	rd->address = g_strdup(address);
	rd->port = (str = g_hash_table_lookup(optlist, "port")) ? atoi(str) : 0;
	rd->use_ssl = g_hash_table_lookup(optlist, "ssl") != NULL;
	signal_emit("xmpp registration started", 2, rd->username, rd->domain);
	start_registration(rd);
	cmd_params_free(free_arg);
}

/* SYNTAX: XMPPUNREGISTER -yes */
static void
cmd_xmppunregister(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	LmMessage *lmsg;
	LmMessageNode *node;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 0 | PARAM_FLAG_OPTIONS, 
	    "xmppunregister", &optlist))
		return;
	if (g_hash_table_lookup(optlist, "yes") == NULL)
		cmd_param_error(CMDERR_NOT_GOOD_IDEA);
	lmsg = lm_message_new_with_sub_type(NULL,
	    LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_REGISTER);
	lm_message_node_add_child(node, "remove", NULL);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
	cmd_params_free(free_arg);
}

/* SYNTAX: XMPPPASSWD -yes <old_password> <new_password> */
static void
cmd_xmpppasswd(const char *data, SERVER_REC *server, WI_ITEM_REC *item)
{
	GHashTable *optlist;
	char *old_password, *new_password, *recoded;
	LmMessage *lmsg;
	LmMessageNode *node;
	void *free_arg;

	CMD_XMPP_SERVER(server);
	if (!cmd_get_params(data, &free_arg, 2 | PARAM_FLAG_OPTIONS,
	    "xmpppasswd", &optlist, &old_password, &new_password))
		return;
	if (g_hash_table_lookup(optlist, "yes") == NULL)
		cmd_param_error(CMDERR_NOT_GOOD_IDEA);
	if (strcmp(old_password, server->connrec->password) != 0)
		cmd_param_error(CMDERR_NOT_ENOUGH_PARAMS);
	lmsg = lm_message_new_with_sub_type(XMPP_SERVER(server)->domain,
	     LM_MESSAGE_TYPE_IQ, LM_MESSAGE_SUB_TYPE_SET);
	node = lm_message_node_add_child(lmsg->node, "query", NULL);
	lm_message_node_set_attribute(node, XMLNS, XMLNS_REGISTER);
	recoded = xmpp_recode_out(XMPP_SERVER(server)->user);
	lm_message_node_add_child(node, "username", recoded);
	g_free(recoded);
	recoded = xmpp_recode_out(new_password);
	lm_message_node_add_child(node, "password", recoded);
	g_free(recoded);
	signal_emit("xmpp send iq", 2, server, lmsg);
	lm_message_unref(lmsg);
	cmd_params_free(free_arg);
}

void
registration_init(void)
{
	register_data = NULL;
	command_bind("xmppregister", NULL, (SIGNAL_FUNC)cmd_xmppregister);
	command_bind("xmppunregister", NULL, (SIGNAL_FUNC)cmd_xmppunregister);
	command_set_options("xmppunregister", "yes");
	command_bind("xmpppasswd", NULL, (SIGNAL_FUNC)cmd_xmpppasswd);
	command_set_options("xmpppasswd", "yes");
	disco_add_feature(XMLNS_REGISTRATION);
}

void
registration_deinit(void)
{
	GSList *tmp, *next;

	command_unbind("xmppregister", (SIGNAL_FUNC)cmd_xmppregister);
	command_unbind("xmppunregister", (SIGNAL_FUNC)cmd_xmppunregister);
	command_unbind("xmpppasswd", (SIGNAL_FUNC)cmd_xmpppasswd);
	for (tmp = register_data; tmp != NULL; tmp = next) {
		next = tmp->next;
		rd_cleanup((struct register_data *)tmp->data);
	}
}
