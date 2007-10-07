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

#include <string.h>

#include "module.h"
#include "nicklist.h"
#include "signals.h"

#include "xmpp-queries.h"
#include "xmpp-channels.h"
#include "xmpp-rosters.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

QUERY_REC *
xmpp_query_create(const char *server_tag, const char *data, int automatic)
{
	XMPP_QUERY_REC *rec, *rec_tmp;
	XMPP_SERVER_REC *server;
	const char *channel_name;

	g_return_val_if_fail(server_tag != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);

	server = XMPP_SERVER(server_find_tag(server_tag));
	if (server == NULL)
		return NULL;

	rec = g_new0(XMPP_QUERY_REC, 1);
	rec->chat_type = XMPP_PROTOCOL;

	channel_name = NULL;
	signal_emit("xmpp windows get active channel", 1, &channel_name);

	/* query created from a channel */
	if (channel_name != NULL) {
		XMPP_CHANNEL_REC *channel;
		NICK_REC *nick;

		channel = xmpp_channel_find(server, channel_name);
		if (channel != NULL) {
			nick = nicklist_find(CHANNEL(channel), data);
			if (nick != NULL)
				rec->name = g_strdup(nick->host);
		}
	}
	
	if (rec->name == NULL)
		rec->name = xmpp_get_full_jid(server, data);

	if (rec->name != NULL) {
		/* test if the query already exist */
		rec_tmp = xmpp_query_find(server, rec->name);
		if (rec_tmp != NULL) {
			g_free(rec->name);
			g_free(rec);
			signal_emit("xmpp query raise", 2, server, rec_tmp);
			return NULL;
		}
	} else
		rec->name = g_strdup(data);

	rec->server_tag = g_strdup(server_tag);
	query_init((QUERY_REC *)rec, automatic);

	rec->composing_time = 0;
	rec->composing_visible = FALSE;

	return (QUERY_REC *)rec;
}
