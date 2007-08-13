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
#include "formats.h"

FORMAT_REC fecommon_xmpp_formats[] = {
    { MODULE_NAME, "XMPP", 0 },

    /* ---- */
	{ NULL, "Roster", 0 },

	{ "roster_group", "{hilight $0}:", 1, { 0 } },
	{ "roster_nick", "  ({hilight $0}) {nick $1} $2", 3, { 0, 0, 0 } },
	{ "roster_ressource", "[$0{nick $1}({hilight $2)}$3]", 4 , { 0, 0, 0, 0 } },
	{ "roster_ressource_away", "({hilight $0})", 1, { 0 } },
	{ "roster_ressource_away_status", ": {comment $3}", 1 , { 0 } },
	{ "begin_of_roster", " ", 0 },
	{ "end_of_roster", "End of /ROSTER list", 0},
    { "not_in_roster", "{nick $0} isn't in the roster.", 1 , { 0 } },

    /* ---- */
    { NULL, "Subscription", 0 },

    { "suscribe", "{nick $0} wants to subscribe to your presence. {comment $1}", 2, { 0, 0 } },
    { "suscribed", "You can see {nick $0} presence now.", 1 , { 0 } },
    { "unsuscribe", "{nick $0} doesn't want to see your presence anymore.", 1 , { 0 } },
    { "unsuscribed", "{nick $0} doesn't want you to see his/her presence anymore.", 1 , { 0 } },

    /* ---- */
    { NULL, "Misc", 0 },

    { "default_event", "$1 $2", 3, { 0, 0, 0 } },
    { "default_error", "ERROR $1 $2", 3, { 0, 0, 0 } },

    { NULL, NULL, 0 }
};
