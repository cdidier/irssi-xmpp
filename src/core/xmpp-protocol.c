/*
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

#include "module.h"
#include "signals.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"

#ifdef DEBUG
#include "fe-common/core/printtext.h"
#include "levels.h"
#endif

#include <sys/utsname.h>

gchar *
xmpp_recode(const gchar *str, const int mode)
{
    const char *charset = "UTF-8";
    G_CONST_RETURN char *local_charset;
    gchar *recoded;
    gboolean is_utf8 = FALSE;

    if (!str)
        return NULL;

    local_charset = settings_get_str("term_charset");
    if (!local_charset)
        is_utf8 = g_get_charset(&local_charset);

    if (is_utf8 || g_strcasecmp(local_charset, charset) == 0)
        goto avoid_recode;

    if (mode == XMPP_RECODE_IN)
        recoded = g_convert(str, -1, local_charset, charset, NULL, NULL, NULL);
    else
        recoded = g_convert(str, -1, charset, local_charset, NULL, NULL, NULL);

    if (!recoded)
        goto avoid_recode;

    return recoded;

avoid_recode:
    return g_strdup(str);
}

gchar *
xmpp_jid_get_username(const gchar *jid)
{
   gchar *pos = g_utf8_strchr(jid, -1, '@');
   if (pos)
       return g_strndup(jid, pos - jid);
   else {
        /* if server unspecified, strip ressource part */
        pos = g_utf8_strchr(jid, -1, '/');
        if (pos)
            return g_strndup(jid, pos - jid);
        else
            return g_strdup(jid);
    }
}

gchar *
xmpp_jid_get_ressource(const gchar *jid)
{
    gchar *pos = g_utf8_strchr(jid, -1, '/');
    if (pos)
        return g_strdup(pos + 1);
    else
        return NULL;
}

gchar *
xmpp_jid_strip_ressource(const gchar *jid)
{
    gchar *pos = g_utf8_strchr(jid, -1, '/');
    if (pos)
        return g_strndup(jid, pos - jid);
    else
        return g_strdup(jid);
}

gboolean
xmpp_jid_have_ressource(const gchar *jid)
{
    if (g_utf8_strchr(jid, -1, '/'))
        return TRUE;
    else
        return FALSE;
}

gboolean
xmpp_jid_have_address(const gchar *jid)
{
    if (g_utf8_strchr(jid, -1, '@'))
        return TRUE;
    else
        return FALSE;
}

gboolean
xmpp_priority_out_of_bound(const int priority)
{
    if ((-128 <= priority) && (priority <= 127))
        return FALSE;
    return TRUE;
}

void
xmpp_send_message_chat(XMPP_SERVER_REC *server, const gchar *to_jid,
    const gchar *message)
{
    LmMessage *msg;
    gchar *to_jid_recoded, *message_recoded;
    GError *error = NULL;

    to_jid_recoded = xmpp_recode(to_jid, XMPP_RECODE_OUT);
    message_recoded = xmpp_recode(message, XMPP_RECODE_OUT);

    msg = lm_message_new_with_sub_type(to_jid_recoded, LM_MESSAGE_TYPE_MESSAGE,
        LM_MESSAGE_SUB_TYPE_CHAT);
    lm_message_node_add_child(lm_message_get_node(msg), "body",
        message_recoded);

    lm_connection_send(server->lmconn, msg, &error);
    lm_message_unref(msg);

    if (error) {
        signal_emit("message error", 2, server, error->message);
        g_free(error);
    }

    g_free(to_jid_recoded);
    g_free(message_recoded);
}

void
xmpp_set_presence(XMPP_SERVER_REC *server, const int show, const gchar *status)
{
    LmMessage *msg;
    LmMessageNode *root;
    gchar *status_recoded, *priority;
    gboolean is_available = FALSE;

    msg = lm_message_new(NULL, LM_MESSAGE_TYPE_PRESENCE);
    root = lm_message_get_node(msg);

    if (show == XMPP_PRESENCE_AWAY)
        lm_message_node_add_child(root, "show",
            xmpp_presence_show[XMPP_PRESENCE_AWAY]); 

    else if (show == XMPP_PRESENCE_CHAT)
        lm_message_node_add_child(root, "show",
            xmpp_presence_show[XMPP_PRESENCE_CHAT]); 

    else if (show == XMPP_PRESENCE_DND)
        lm_message_node_add_child(root, "show",
            xmpp_presence_show[XMPP_PRESENCE_DND]);

    else if (show == XMPP_PRESENCE_XA)
        lm_message_node_add_child(root, "show",
            xmpp_presence_show[XMPP_PRESENCE_XA]);

    else
        is_available = TRUE;

    if (is_available) {

		if (server->usermode_away)
    		signal_emit("event 305", 2, server, server->nick); // unaway

		server->show = XMPP_PRESENCE_AVAILABLE;
        g_free_and_null(server->away_reason);

    } else {

		signal_emit("event 306", 2, server, server->nick); // away

		server->show = show;
        g_free(server->away_reason);
        server->away_reason = g_strdup(status);

    }

    if (server->away_reason) {
        status_recoded = xmpp_recode(server->away_reason, XMPP_RECODE_OUT);
        lm_message_node_add_child(root, "status", status_recoded);
        g_free(status_recoded);
    }

    priority = g_strdup_printf("%d", server->priority);
    lm_message_node_add_child(root, "priority", priority);
    g_free(priority);

    lm_connection_send(server->lmconn, msg, NULL);
    lm_message_unref(msg);
}

/*
 * XEP-0092: Software Version
 * http://www.xmpp.org/extensions/xep-0092.html
 */
void
xmpp_version_send(XMPP_SERVER_REC *server, const gchar *to_jid, 
    const gchar *msg_id)
{
    LmMessage *msg;
    LmMessageNode *root, *query;
    struct utsname u;

    if (!settings_get_bool("xmpp_send_version"))
        return;

    msg = lm_message_new_with_sub_type(to_jid, LM_MESSAGE_TYPE_IQ,
        LM_MESSAGE_SUB_TYPE_RESULT);
    root = lm_message_get_node(msg);

    if (msg_id)
        lm_message_node_set_attribute(root, "id", msg_id);

    query = lm_message_node_add_child(root, "query", NULL);
    lm_message_node_set_attribute(query, "xmlns", "jabber:iq:version");

    lm_message_node_add_child(query, "name", IRSSI_XMPP_PACKAGE);
    lm_message_node_add_child(query, "version", IRSSI_XMPP_VERSION);

    if (uname(&u) == 0)
        lm_message_node_add_child(query, "os", u.sysname);

    lm_connection_send(server->lmconn, msg, NULL);
    lm_message_unref(msg);
}

/*
 * Incoming messages handlers
 */
static LmHandlerResult
xmpp_handle_message(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
    XMPP_SERVER_REC *server;
    LmMessageNode *root, *body;
    const gchar *message, *jid;
    gchar *message_recoded;

    server = XMPP_SERVER(user_data);
    root = lm_message_get_node(msg);

    if (lm_message_get_sub_type(msg) == LM_MESSAGE_SUB_TYPE_ERROR)
        goto err_handle_message;

#ifdef DEBUG
    printtext(server, NULL, MSGLEVEL_CRAP, "MESSAGE: %s", lm_message_node_to_string(lm_message_get_node(msg)));
#endif

    body = lm_message_node_get_child(root, "body");
    if (!body)
       return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

    message = lm_message_node_get_value(body);
    message_recoded = xmpp_recode(message, XMPP_RECODE_IN);
    jid = lm_message_node_get_attribute(root, "from");

    signal_emit("message private", 4, server, message_recoded, jid, jid);

    g_free(message_recoded);
    return LM_HANDLER_RESULT_REMOVE_MESSAGE;

err_handle_message:
    signal_emit("message error", 2, server,
        lm_message_node_get_attribute(root, "from"),
        lm_message_node_get_attribute(
            lm_message_node_get_child(root, "error"), "code"));

    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
xmpp_handle_presence(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
    XMPP_SERVER_REC *server;
    LmMessageNode *root, *show_node, *status_node, *priority_node;
    const gchar *show, *priority;
    gchar *jid, *status;

    server = XMPP_SERVER(user_data);
    root = lm_message_get_node(msg);

    jid = xmpp_recode(lm_message_node_get_attribute(root, "from"),
        XMPP_RECODE_IN);
    status = NULL;

    /* the user is disconnecting */
    if (lm_message_get_sub_type(msg) == LM_MESSAGE_SUB_TYPE_UNAVAILABLE) {

        status_node = lm_message_node_get_child(root, "status");
        if (status_node)
            status = xmpp_recode(lm_message_node_get_value(status_node),
                XMPP_RECODE_IN);

        xmpp_roster_presence_unavailable(server, jid, status);

    /* an error occured when the server try to get the pressence of an user */
    } else if (lm_message_get_sub_type(msg) == LM_MESSAGE_SUB_TYPE_ERROR) {

        xmpp_roster_presence_error(server, jid);

    /* the user wants to add you in his/her roster */
    } else if (lm_message_get_sub_type(msg) == LM_MESSAGE_SUB_TYPE_SUBSCRIBE) {

        status_node = lm_message_node_get_child(root, "status");
        if (status_node)
            status = xmpp_recode(lm_message_node_get_value(status_node),
                XMPP_RECODE_IN);

        signal_emit("xmpp event subscribe", 3, server, jid, status);

    } else if (lm_message_get_sub_type(msg)
                == LM_MESSAGE_SUB_TYPE_UNSUBSCRIBE) {

        signal_emit("xmpp event unsubscribe", 2, server, jid);

    } else if (lm_message_get_sub_type(msg)
                == LM_MESSAGE_SUB_TYPE_SUBSCRIBED) {

        signal_emit("xmpp event subscribed", 2, server, jid);

    } else if (lm_message_get_sub_type(msg)
                == LM_MESSAGE_SUB_TYPE_UNSUBSCRIBED) {

        signal_emit("xmpp event unsubscribed", 2, server, jid);

    /* the user change his presence */
    } else if (!lm_message_node_get_attribute(root, "type")) {

        show_node = lm_message_node_get_child(root, "show");
        if (show_node)
            show = lm_message_node_get_value(show_node);
        else
            show = NULL;
    
        status_node = lm_message_node_get_child(root, "status");
        if (status_node)
            status = xmpp_recode(lm_message_node_get_value(status_node),
                XMPP_RECODE_IN);

        priority_node = lm_message_node_get_child(root, "priority");
        if (priority_node)
            priority = lm_message_node_get_value(priority_node);
        else
            priority = NULL;

        xmpp_roster_update_presence(server, jid, show, status, priority);

    }

    g_free(status);
    g_free(jid);

#ifdef DEBUG
    printtext(server, NULL, MSGLEVEL_CRAP, "PRESENCE: %s", lm_message_node_to_string(lm_message_get_node(msg)));
#endif

    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

static LmHandlerResult
xmpp_handle_iq(LmMessageHandler *handler, LmConnection *connection,
    LmMessage *msg, gpointer user_data)
{
    XMPP_SERVER_REC *server;
    LmMessageNode *root, *query;
    LmMessageSubType type;
    const gchar *xmlns, *msg_id, *jid;

    server = XMPP_SERVER(user_data);
    root = lm_message_get_node(msg);
    type = lm_message_get_sub_type(msg);
    jid = lm_message_node_get_attribute(root, "from");
    msg_id = lm_message_node_get_attribute(root, "id");

#ifdef DEBUG
    printtext(server, NULL, MSGLEVEL_CRAP, "IQ: %s", lm_message_node_to_string(lm_message_get_node(msg)));
#endif

    query = lm_message_node_get_child(root, "query");
    if (!query)
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

    xmlns = lm_message_node_get_attribute(query, "xmlns");
    if (!xmlns)
        return LM_HANDLER_RESULT_ALLOW_MORE_HANDLERS;

    if (type == LM_MESSAGE_SUB_TYPE_GET) {

        if (g_ascii_strcasecmp(xmlns, "jabber:iq:version") == 0)
            xmpp_version_send(server, jid, msg_id);

    } else if (type == LM_MESSAGE_SUB_TYPE_RESULT
            || type == LM_MESSAGE_SUB_TYPE_SET) {

        if (g_ascii_strcasecmp(xmlns, "jabber:iq:roster") == 0)
            xmpp_roster_update(server, query);

    }

    return LM_HANDLER_RESULT_REMOVE_MESSAGE;
}

void
xmpp_register_handlers(XMPP_SERVER_REC *server)
{
    LmMessageHandler *hmessage, *hpresence, *hiq;

	/* handle message */
    hmessage = lm_message_handler_new(
        (LmHandleMessageFunction) xmpp_handle_message, (gpointer) server, NULL);
    lm_connection_register_message_handler(server->lmconn, hmessage,
        LM_MESSAGE_TYPE_MESSAGE, LM_HANDLER_PRIORITY_NORMAL);
    lm_message_handler_unref(hmessage);

	/* handle presence */
    hpresence = lm_message_handler_new(
        (LmHandleMessageFunction) xmpp_handle_presence, (gpointer) server,
        NULL);
    lm_connection_register_message_handler(server->lmconn, hpresence,
        LM_MESSAGE_TYPE_PRESENCE, LM_HANDLER_PRIORITY_NORMAL);
    lm_message_handler_unref(hpresence);

	/* handle iq */
    hiq = lm_message_handler_new(
        (LmHandleMessageFunction) xmpp_handle_iq, (gpointer) server, NULL);
    lm_connection_register_message_handler(server->lmconn, hiq,
        LM_MESSAGE_TYPE_IQ, LM_HANDLER_PRIORITY_NORMAL);
    lm_message_handler_unref(hiq);
}
