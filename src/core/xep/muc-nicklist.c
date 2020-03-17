/*
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

#include "module.h"
#include <irssi/src/core/signals.h>

#include "rosters.h"
#include "muc-affiliation.h"
#include "muc-nicklist.h"
#include "muc-role.h"

XMPP_NICK_REC *
xmpp_nicklist_insert(MUC_REC *channel, const char *nickname,
    const char *full_jid)
{
	XMPP_NICK_REC *rec;

	g_return_val_if_fail(IS_MUC(channel), NULL);
	g_return_val_if_fail(nickname != NULL, NULL);
	rec = g_new0(XMPP_NICK_REC, 1);
	rec->nick = g_strdup(nickname);
	rec->host = (full_jid != NULL) ?
	    g_strdup(full_jid) : g_strconcat(channel->name, "/", rec->nick, (void *)NULL);
	rec->show = XMPP_PRESENCE_AVAILABLE;
	rec->status = NULL;
	rec->affiliation = XMPP_AFFILIATION_NONE;
	rec->role = XMPP_ROLE_NONE;
	nicklist_insert(CHANNEL(channel), (NICK_REC *)rec);
	return rec;
}

static void
nick_hash_add(CHANNEL_REC *channel, NICK_REC *nick)
{
	NICK_REC *list;

	nick->next = NULL;
	list = g_hash_table_lookup(channel->nicks, nick->nick);
        if (list == NULL)
		g_hash_table_insert(channel->nicks, nick->nick, nick);
	else {
                /* multiple nicks with same name */
		while (list->next != NULL)
			list = list->next;
		list->next = nick;
	}
	/* move our own nick to beginning of the nick list.. */
	if (nick == channel->ownnick)
		nicklist_set_own(channel, nick);
}

static void
nick_hash_remove(CHANNEL_REC *channel, NICK_REC *nick)
{
	NICK_REC *list;

	list = g_hash_table_lookup(channel->nicks, nick->nick);
	if (list == NULL)
		return;
	if (list == nick || list->next == NULL) {
		g_hash_table_remove(channel->nicks, nick->nick);
		if (list->next != NULL)
			g_hash_table_insert(channel->nicks, nick->next->nick,
			    nick->next);
	} else {
		while (list->next != nick)
			list = list->next;
		list->next = nick->next;
	}
}

void
xmpp_nicklist_rename(MUC_REC *channel, XMPP_NICK_REC *nick,
    const char *oldnick, const char *newnick)
{

	g_return_if_fail(IS_MUC(channel));
	g_return_if_fail(IS_XMPP_NICK(nick));
	g_return_if_fail(oldnick != NULL);
	g_return_if_fail(newnick != NULL);
	/* remove old nick from hash table */
	nick_hash_remove(CHANNEL(channel), NICK(nick));
	g_free(nick->nick);
	nick->nick = g_strdup(newnick);
	/* add new nick to hash table */
	nick_hash_add(CHANNEL(channel), NICK(nick));
	signal_emit("nicklist changed", 3, channel, nick, oldnick);
	if (strcmp(oldnick, channel->nick) == 0) {
		nicklist_set_own(CHANNEL(channel), NICK(nick));
		g_free(channel->nick);
		channel->nick = g_strdup(newnick);
	}
}

gboolean
xmpp_nicklist_modes_changed(XMPP_NICK_REC *nick, int affiliation, int role)
{
	g_return_val_if_fail(IS_XMPP_NICK(nick), FALSE);
	return nick->affiliation != affiliation || nick->role != role;
}

void
xmpp_nicklist_set_modes(XMPP_NICK_REC *nick, int affiliation, int role)
{
	g_return_if_fail(IS_XMPP_NICK(nick));

	nick->affiliation = affiliation;
	nick->role = role;
	switch (affiliation) {
	case XMPP_AFFILIATION_OWNER:
		nick->prefixes[0] = '&';
		nick->prefixes[1] = '\0';
		nick->op = TRUE;
		break;
	case XMPP_AFFILIATION_ADMIN:
		nick->prefixes[0] = '\0';
		nick->op = TRUE;
		break;
	default:
		nick->prefixes[0] = '\0';
		nick->op = FALSE;
	}
	switch (role) {
	case XMPP_ROLE_MODERATOR:
		nick->voice = TRUE;
		nick->halfop = TRUE;
		break;
	case XMPP_ROLE_PARTICIPANT:
		nick->halfop = FALSE;
		nick->voice = TRUE;
		break;
	default:
		nick->halfop = FALSE;
		nick->voice = FALSE;
	}
}

void
xmpp_nicklist_set_presence(XMPP_NICK_REC *nick, int show, const char *status)
{
	g_return_if_fail(IS_XMPP_NICK(nick));
	nick->show = show;
	g_free(nick->status);
	nick->status = g_strdup(status);
}

static void
sig_nicklist_remove(MUC_REC *channel, XMPP_NICK_REC *nick)
{
	if (!IS_MUC(channel) || !IS_XMPP_NICK(nick))
		return;
	g_free(nick->status);
}

void
muc_nicklist_init(void)
{
	signal_add("nicklist remove", sig_nicklist_remove);
}

void
muc_nicklist_deinit(void)
{
	signal_remove("nicklist remove", sig_nicklist_remove);
}
