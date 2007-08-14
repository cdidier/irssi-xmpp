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

#include "xmpp-queries.h"
#include "xmpp-servers.h"
#include "xmpp-protocol.h"
#include "xmpp-rosters.h"

#include <string.h>

QUERY_REC *
xmpp_query_create(const char *server_tag, const char *nick, int automatic)
{
    QUERY_REC *rec, *rec_tmp;
    XMPP_SERVER_REC *server;
    XmppRosterUser *user;
    XmppRosterRessource *ressource;

    g_return_val_if_fail(nick != NULL, NULL);

    rec = g_new0(QUERY_REC, 1);
    rec->chat_type = XMPP_PROTOCOL;
    rec->name = NULL;

    /* if unspecified, open queries with the highest ressource */
    if (!xmpp_jid_have_ressource(nick)) {
        server = XMPP_SERVER(server_find_tag(server_tag));
        if (server == NULL)
            goto query_pass_ressource;

        user = xmpp_find_user_from_groups(server->roster, nick, NULL);
        if ((user == NULL) || (user->ressources == NULL))
            goto query_pass_ressource;

        ressource = user->ressources->data;
        if (ressource->name != NULL)
           rec->name = g_strdup_printf("%s/%s", nick, ressource->name);

        rec_tmp = xmpp_query_find(server, rec->name);
        if (rec_tmp != NULL) {
            g_free(rec->name);
            g_free(rec);
            rec = rec_tmp;
            goto query_raise;
        }
    }

query_pass_ressource:
    if (rec->name == NULL)
        rec->name = g_strdup(nick);

    rec->server_tag = g_strdup(server_tag);
    query_init(rec, automatic);

    return rec;

query_raise:
    signal_emit("xmpp event raise query", 2, server, rec);
    return NULL;
}
