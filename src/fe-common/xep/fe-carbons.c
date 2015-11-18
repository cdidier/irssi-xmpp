/*
 * Copyright (C) 2017 Stephen Paul Weber
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

#include "module.h"
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "settings.h"
#include "signals.h"
#include "window-items.h"
#include "fe-messages.h"
#include "fe-queries.h"
#include "fe-common/core/module-formats.h"
#include "fe-common/core/fe-messages.h"
#include "fe-common/irc/module-formats.h"

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "xep/muc.h"

static void
sig_message_carbons_sent(SERVER_REC *server, const char *msg, const char *nick,
                         const char *target, gpointer gpointer_type)
{
	void *item;
	char *freemsg = NULL;
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);

	type = GPOINTER_TO_INT(gpointer_type);
	level = MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT
	    | (type == SEND_TARGET_CHANNEL ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)get_muc((XMPP_SERVER_REC *)server, target) :
	    query_find(server, target);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);

	if (type == SEND_TARGET_CHANNEL) {
		char *nickmode = channel_get_nickmode(item, nick);
		printformat_module(CORE_MODULE_NAME, server, target,
		    level, TXT_OWN_MSG_CHANNEL, nick, target, msg, nickmode);
	} else if(item) { // If no query, why do we want to see carbons?
		printformat_module(CORE_MODULE_NAME, server, target,
		    level, TXT_OWN_MSG_PRIVATE_QUERY, target, msg, nick);
	}

	g_free_not_null(freemsg);
}

void
fe_carbons_init(void)
{
	signal_add("message xmpp carbons sent", sig_message_carbons_sent);
}

void
fe_carbons_deinit(void)
{
	signal_remove("message xmpp carbons sent", sig_message_carbons_sent);
}
