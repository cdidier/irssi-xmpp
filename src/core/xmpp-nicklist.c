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
#include "signals.h"

#include "xmpp-nicklist.h"
#include "xmpp-channels.h"
#include "xmpp-rosters.h"

const char *xmpp_nicklist_affiliation[] = {
	"none",
	"owner",
	"admin",
	"member",
	"outcast",
	NULL
};

const char *xmpp_nicklist_role[] = {
	"none",
	"moderator",
	"participant",
	"visitor",
	NULL
};

XMPP_NICK_REC *
xmpp_nicklist_insert(XMPP_CHANNEL_REC *channel, const char *nick_name,
    const char *full_jid)
{
	XMPP_NICK_REC *rec;

	g_return_val_if_fail(IS_XMPP_CHANNEL(channel), NULL);
	g_return_val_if_fail(nick_name != NULL, NULL);

	rec = g_new0(XMPP_NICK_REC, 1);

	rec->nick = g_strdup(nick_name);
	rec->host = (full_jid != NULL) ?
	    g_strdup(full_jid) : g_strconcat(channel->name, "/", rec->nick, NULL);
	rec->show = XMPP_PRESENCE_AVAILABLE;
	rec->status = NULL;
	rec->affiliation = XMPP_NICKLIST_AFFILIATION_NONE;
	rec->role = XMPP_NICKLIST_ROLE_NONE;

	nicklist_insert(CHANNEL(channel), (NICK_REC *)rec);

	if (strcmp(rec->nick, channel->nick) == 0) {
		nicklist_set_own(CHANNEL(channel), NICK(rec));
		channel->chanop = channel->ownnick->op;
	}

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

	if (nick == channel->ownnick) {
                /* move our own nick to beginning of the nick list.. */
		nicklist_set_own(channel, nick);
	}
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
		if (list->next != NULL) {
			g_hash_table_insert(channel->nicks, nick->next->nick,
					    nick->next);
		}
	} else {
		while (list->next != nick)
			list = list->next;
		list->next = nick->next;
	}
}

void
xmpp_nicklist_rename(XMPP_CHANNEL_REC *channel, XMPP_NICK_REC *nick,
    const char *oldnick, const char *newnick)
{

	g_return_if_fail(IS_XMPP_CHANNEL(channel));
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
}

int
xmpp_nicklist_get_affiliation(const char *affiliation)
{
	if (affiliation != NULL) {
		if (g_ascii_strcasecmp(affiliation,
		    xmpp_nicklist_affiliation[XMPP_NICKLIST_AFFILIATION_OWNER]) == 0)
			return XMPP_NICKLIST_AFFILIATION_OWNER;

		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_nicklist_affiliation[XMPP_NICKLIST_AFFILIATION_ADMIN]) == 0)
			return XMPP_NICKLIST_AFFILIATION_ADMIN;

		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_nicklist_affiliation[XMPP_NICKLIST_AFFILIATION_MEMBER]) == 0)
			return XMPP_NICKLIST_AFFILIATION_MEMBER;

		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_nicklist_affiliation[XMPP_NICKLIST_AFFILIATION_OUTCAST]) == 0)
			return XMPP_NICKLIST_AFFILIATION_OUTCAST;
	}
	return XMPP_NICKLIST_AFFILIATION_NONE;
}

int
xmpp_nicklist_get_role(const char *role)
{
	if (role != NULL) {
		if (g_ascii_strcasecmp(role,
		    xmpp_nicklist_role[XMPP_NICKLIST_ROLE_MODERATOR]) == 0)
			return XMPP_NICKLIST_ROLE_MODERATOR;

		else if (g_ascii_strcasecmp(role,
		    xmpp_nicklist_role[XMPP_NICKLIST_ROLE_PARTICIPANT]) == 0)
			return XMPP_NICKLIST_ROLE_PARTICIPANT;

		else if (g_ascii_strcasecmp(role,
		    xmpp_nicklist_role[XMPP_NICKLIST_ROLE_VISITOR]) == 0)
			return XMPP_NICKLIST_ROLE_VISITOR;
	}
	return XMPP_NICKLIST_ROLE_NONE;
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
	case XMPP_NICKLIST_AFFILIATION_OWNER:
		nick->other = '&';
		nick->op = TRUE;
		break;
	case XMPP_NICKLIST_AFFILIATION_ADMIN:
		nick->other = NULL;
		nick->op = TRUE;
		break;
	default:
		nick->other = NULL;
		nick->op = FALSE;
	}

	switch (role) {
	case XMPP_NICKLIST_ROLE_MODERATOR:
		nick->voice = TRUE;
		nick->halfop = TRUE;
		break;
	case XMPP_NICKLIST_ROLE_PARTICIPANT:
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
sig_nicklist_remove(XMPP_CHANNEL_REC *channel, XMPP_NICK_REC *nick)
{
	if (!IS_XMPP_CHANNEL(channel) || !IS_XMPP_NICK(nick))
		return;

	g_free(nick->status);
}

void
xmpp_nicklist_init(void)
{
	signal_add("nicklist remove", (SIGNAL_FUNC)sig_nicklist_remove);
}

void
xmpp_nicklist_deinit(void)
{
	signal_remove("nicklist remove", (SIGNAL_FUNC)sig_nicklist_remove);
}
