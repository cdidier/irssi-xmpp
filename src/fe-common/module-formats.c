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

#include "module.h"
#include "formats.h"

FORMAT_REC fecommon_xmpp_formats[] = {
	{ MODULE_NAME, "XMPP", 0, { 0 } },

	/* ---- */
	{ NULL, "Format", 0, { 0 } },

	{ "format_name", "{nick $0} {nickhost $1}", 2, { 0, 0 } },
	{ "format_jid", "{nick $0}", 1, { 0 } },
	{ "format_resource", "{comment $0{hilight $1}($2)$3}", 4, { 0, 0, 0, 0 } },
	{ "format_resource_show", "($0)", 1, { 0 } },
	{ "format_resource_status", ": $0", 1, { 0 } },
	{ "format_subscription", "(subscription: $0)", 1, { 0 } },

	/* ---- */
	{ NULL, "Roster", 0, { 0 } },

	{ "roster_group", " {hilight $0}:", 1, { 0 } },
	{ "roster_contact", "   ({hilight $0}) $1 $2 $3", 4, { 0, 0, 0, 0 } },
	{ "begin_of_roster", "ROSTER: {nick $0} $1 $2", 3, { 0, 0, 0 } },
	{ "end_of_roster", "End of ROSTER", 0, { 0 } },
	{ "not_in_roster", "{nick $0}: not in the roster", 1, { 0 } },

	/* ---- */
	{ NULL, "Subscription", 0, { 0 } },

	{ "suscribe", "$0: wants to subscribe to your presence {comment $1} (accept or deny?)", 2, { 0, 0 } },
	{ "suscribed", "$0: wants you to see his/her presence", 1, { 0 } },
	{ "unsuscribe", "$0: doesn't want to see your presence anymore", 1 , { 0 } },
	{ "unsuscribed", "$0: doesn't want you to see his/her presence anymore", 1 , { 0 } },

	/* ---- */
	{ NULL, "Message", 0, { 0 } },

	{ "message_event", "$0: $1", 2,  { 0, 0 } },
	{ "message_not_delivered", "$0: cannot deliver message {comment $1}", 2,  { 0, 0 } },
	{ "message_timestamp", "[{timestamp $0}] $1", 2, { 0, 0 } },

	/* ---- */
	{ NULL, "Queries", 0, { 0 } },

	{ "query_aka", "{nick $0}: Also known as {nick $1}", 2, { 0, 0 } },

	/* ---- */
	{ NULL, "Channel", 0, { 0 } },

	{ "joinerror", "Cannot join to room {channel $0} {comment $1}", 2, { 0, 0 } },
	{ "destroyerror", "Cannot destroy room {channel $0} {comment $1}", 2, { 0, 0 } },

	/* ---- */
	{ NULL, "Presence", 0, { 0 } },

	{ "presence_change", "$0: is now {hilight $1}", 2, { 0, 0 } },
	{ "presence_change_reason", "$0: is now {hilight $1} {comment $2}", 3, { 0, 0, 0 } },

	/* ---- */
	{ NULL, "VCard", 0, { 0 } },

	{ "vcard", "{nick $0} {nickhost $1}", 2, { 0, 0 } },
	{ "vcard_value", "  $0: $1", 2, { 0, 0 } },
	{ "vcard_subvalue", "    $0: $1", 2, { 0, 0 } },
	{ "end_of_vcard", "End of VCARD", 0, { 0 } },

	/* ---- */
	{ NULL, "Misc", 0, { 0 } },

	{ "raw_in_header", "RECV[$0]:", 1, { 0 } },
	{ "raw_out_header", "SEND[$0]:", 1, { 0 } },
	{ "raw_message", "$0", 1, { 0 } },
	{ "default_event", "$1 $2", 3, { 0, 0, 0 } },
	{ "default_error", "ERROR $1 $2", 3, { 0, 0, 0 } },

	{ NULL, "Regisration", 0, { 0 } },

	{ "xmpp_registration_started", "Registering {nick $0@$1}...", 2, { 0, 0 } },
	{ "xmpp_registration_succeed", "Registration of {nick $0@$1} succeeded", 2, { 0, 0 } },
	{ "xmpp_registration_failed", "Registration of {nick $0@$1} failed {comment $2}", 3, { 0, 0, 0 } },

	{ NULL, NULL, 0, { 0 } }
};
