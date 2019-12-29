/*
 * Copyright (C) 2009 Colin DIDIER
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
#include <irssi/src/core/levels.h>
#include "module-formats.h"
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/settings.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/core/window-items.h>
#include <irssi/src/fe-common/core/fe-messages.h>
#include <irssi/src/fe-common/core/fe-queries.h>
#include <irssi/src/fe-common/core/module-formats.h>
#include <irssi/src/fe-common/core/fe-messages.h>
#include <irssi/src/fe-common/irc/module-formats.h>

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "xep/muc.h"

static void
sig_message_delay(SERVER_REC *server, const char *msg, const char *nick,
    const char *target, time_t *t, gpointer gpointer_type)
{
	void *item;
	char *text, *freemsg = NULL;
	char stamp[BUFSIZ];
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
	    query_find(server, nick);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);

	/* MUC */
	if (type == SEND_TARGET_CHANNEL) {
		CHANNEL_REC *chanrec = item;
		int print_channel;
		char *nickmode;

		print_channel = chanrec == NULL ||
		    !window_item_is_active((WI_ITEM_REC *)chanrec);
		if (!print_channel
		    && settings_get_bool("print_active_channel")
		    && window_item_window((WI_ITEM_REC *)chanrec)->items->next
		    != NULL)
			print_channel = TRUE;
		nickmode = channel_get_nickmode(chanrec, nick);

		text = !print_channel ?
		    format_get_text(CORE_MODULE_NAME, NULL, server,
		        target, TXT_PUBMSG, nick, msg, nickmode) :
		    format_get_text(CORE_MODULE_NAME, NULL, server,
		        target, TXT_PUBMSG_CHANNEL, nick, target, msg,
		        nickmode);

		g_free(nickmode);

	/* General */
	} else
		text = format_get_text(CORE_MODULE_NAME, NULL, server,
		    target, item == NULL ? TXT_MSG_PRIVATE :
		    TXT_MSG_PRIVATE_QUERY, nick, nick, msg);

	if (strftime(stamp, sizeof(stamp)-1,
	    settings_get_str("xmpp_timestamp_format"), localtime(t)) == 0)
	stamp[sizeof(stamp)-1] = '\0';

	printformat_module(MODULE_NAME, server, target,
	    level, XMPPTXT_MESSAGE_TIMESTAMP,
	    stamp, text);

	g_free_not_null(freemsg);
	g_free(text);
}

static void
sig_message_delay_action(SERVER_REC *server, const char *msg, const char *nick,
    const char *target, time_t *t, gpointer gpointer_type)
{
	void *item;
	char *text, *freemsg = NULL;
	char stamp[BUFSIZ];
	int level, type;

	g_return_if_fail(server != NULL);
	g_return_if_fail(msg != NULL);
	g_return_if_fail(nick != NULL);
	g_return_if_fail(target != NULL);

	type = GPOINTER_TO_INT(gpointer_type);
	level = MSGLEVEL_ACTIONS | MSGLEVEL_NO_ACT | MSGLEVEL_NOHILIGHT
	    | (type == SEND_TARGET_CHANNEL ? MSGLEVEL_PUBLIC : MSGLEVEL_MSGS);
	item = type == SEND_TARGET_CHANNEL ?
	    (void *)get_muc((XMPP_SERVER_REC *)server, target) :
	    query_find(server, nick);

	if (settings_get_bool("emphasis"))
		msg = freemsg = expand_emphasis(item, msg);

	/* MUC */
	if (type == SEND_TARGET_CHANNEL) {
		if (item && window_item_is_active(item))
			text = format_get_text(IRC_MODULE_NAME, NULL, server,
			    target, IRCTXT_ACTION_PUBLIC, nick, msg);
		else
			text = format_get_text(IRC_MODULE_NAME, NULL, server,
			    target, IRCTXT_ACTION_PUBLIC_CHANNEL, nick,
			    target, msg);

	/* General */
	} else
		text = format_get_text(IRC_MODULE_NAME, NULL, server,
		    nick, (item == NULL) ? IRCTXT_ACTION_PRIVATE : 
		    IRCTXT_ACTION_PRIVATE_QUERY, nick, nick, msg);

	if (strftime(stamp, sizeof(stamp)-1,
	    settings_get_str("xmpp_timestamp_format"), localtime(t)) == 0)
	stamp[sizeof(stamp)-1] = '\0';

	printformat_module(MODULE_NAME, server, target, level,
	    XMPPTXT_MESSAGE_TIMESTAMP, stamp, text);

	g_free(freemsg);
}

void
fe_delay_init(void)
{
	settings_add_str("xmpp_lookandfeel", "xmpp_timestamp_format",
	    "%Y-%m-%d %H:%M");
	signal_add("message xmpp delay", sig_message_delay);
	signal_add("message xmpp delay action", sig_message_delay_action);
}

void
fe_delay_deinit(void)
{
	signal_remove("message xmpp delay", sig_message_delay);
	signal_remove("message xmpp delay action", sig_message_delay_action);
}
