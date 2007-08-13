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
#include "chat-protocols.h"
#include "chatnets.h"
#include "servers-setup.h"
#include "channels-setup.h"
#include "settings.h"

#include "xmpp-servers.h"
#include "xmpp-commands.h"
#include "xmpp-queries.h"

static CHATNET_REC *
create_chatnet(void)
{
    return g_new0(CHATNET_REC, 1);
}

static SERVER_SETUP_REC *
create_server_setup(void)
{
    return g_new0(SERVER_SETUP_REC, 1);
}

static SERVER_CONNECT_REC *
create_server_connect(void)
{
    return (SERVER_CONNECT_REC *) g_new0(XMPP_SERVER_CONNECT_REC, 1);
}

static CHANNEL_SETUP_REC *
create_channel_setup(void)
{
    return g_new0(CHANNEL_SETUP_REC, 1);
}

static void
destroy_server_connect(SERVER_CONNECT_REC *conn)
{
}

void
xmpp_core_init(void)
{
    CHAT_PROTOCOL_REC *rec;

    rec = g_new0(CHAT_PROTOCOL_REC, 1);
    rec->name = "XMPP";
    rec->fullname = "XMPP, Extensible messaging and presence protocol";
    rec->chatnet = "xmppnet";

    rec->case_insensitive = TRUE;

    rec->create_chatnet = create_chatnet;
    rec->create_server_setup = create_server_setup;
    rec->create_server_connect = create_server_connect;
    rec->create_channel_setup = create_channel_setup;
    rec->destroy_server_connect = destroy_server_connect;

    rec->server_init_connect = xmpp_server_init_connect;
    rec->server_connect = xmpp_server_connect;
/*    rec->channel_create = xmpp_channel_create;*/
    rec->query_create = xmpp_query_create;

    chat_protocol_register(rec);
    g_free(rec);

    xmpp_servers_init();
/*    xmpp_channels_init();*/
    xmpp_commands_init();

    module_register("xmpp", "core");
}

void
xmpp_core_deinit(void) 
{
    xmpp_servers_deinit();
/*    xmpp_channels_deinit();*/
    xmpp_commands_deinit();

    signal_emit("chat protocol deinit", 1, chat_protocol_find("XMPP"));
    chat_protocol_unregister("XMPP");
}
