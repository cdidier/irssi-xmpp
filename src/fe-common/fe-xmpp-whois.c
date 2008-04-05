/*
 * $Id$
 *
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
#include "levels.h"
#include "module-formats.h"
#include "printtext.h"
#include "signals.h"

#include "xmpp-servers.h"
#include "xmpp-rosters-tools.h"
#include "xmpp-tools.h"

static void
sig_version(XMPP_SERVER_REC *server, const char *jid, const char *client,
    const char *version, const char *os)
{
	XMPP_ROSTER_USER_REC *user;
	char *name, *str;

	g_return_if_fail(jid != NULL);

	if (name == NULL && version == NULL && os == NULL)
		return;

	str = g_strconcat("is running ",
	    client != NULL ? client : "",
	    client != NULL && version != NULL ? " " : "",
	    version != NULL ? version : "",
	    (client != NULL || version != NULL) && os != NULL ? " - " : "",
	    os != NULL ? "on " : "",
	    os != NULL ? os : "", NULL);

	user = xmpp_rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_NAME, user->name, jid) :
	    format_get_text(MODULE_NAME, NULL, server, NULL,
	        XMPPTXT_FORMAT_JID, jid);

	printformat_module(MODULE_NAME, server, jid, MSGLEVEL_CRAP,
	    XMPPTXT_MESSAGE_EVENT, name, str);

	g_free(name);
	g_free(str);
}

struct vcard_user_data {
	XMPP_SERVER_REC *server;
	const char *jid;
};

static void
func_vcard_value(const char *key, const char *value, struct vcard_user_data *ud)
{
	printformat_module(MODULE_NAME, ud->server, ud->jid, MSGLEVEL_CRAP,
	    XMPPTXT_VCARD_VALUE, key, value);
}

static void
func_vcard_subvalue(const char *key, const char *value,
    struct vcard_user_data *ud)
{
	printformat_module(MODULE_NAME, ud->server, ud->jid, MSGLEVEL_CRAP,
	    XMPPTXT_VCARD_SUBVALUE, key, value);
}

static void
sig_vcard(XMPP_SERVER_REC *server, const char *jid, GHashTable *ht)
{
	XMPP_ROSTER_USER_REC *user;
	struct vcard_user_data ud;
	char *name;

	user = xmpp_rosters_find_user(server->roster, jid, NULL, NULL);
	name = user != NULL && user->name != NULL ?
	    g_strdup(user->name) : xmpp_strip_resource(jid);
	printformat_module(MODULE_NAME, server, jid, MSGLEVEL_CRAP,
	    XMPPTXT_VCARD, name, jid);
	g_free(name);

	ud.server = server;
	ud.jid = jid;
	g_hash_table_foreach(ht,
	    (void (*)(gpointer, gpointer, gpointer))func_vcard_value, &ud);

	printformat_module(MODULE_NAME, server, jid, MSGLEVEL_CRAP,
	    XMPPTXT_END_OF_VCARD);
}

void
fe_xmpp_whois_init(void)
{
	signal_add("xmpp version", (SIGNAL_FUNC)sig_version);
	signal_add("xmpp vcard", (SIGNAL_FUNC)sig_vcard);
}

void
fe_xmpp_whois_deinit(void)
{   
	signal_remove("xmpp version", (SIGNAL_FUNC)sig_version);
	signal_remove("xmpp vcard", (SIGNAL_FUNC)sig_vcard);
}
