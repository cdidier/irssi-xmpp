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
#include <irssi/src/core/levels.h>
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/signals.h>

#include "xmpp-servers.h"
#include "rosters-tools.h"
#include "tools.h"
#include "../module-formats.h"

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

#if 0

static void
func_vcard_subvalue(const char *key, const char *value,
    struct vcard_user_data *ud)
{
	printformat_module(MODULE_NAME, ud->server, ud->jid, MSGLEVEL_CRAP,
	    XMPPTXT_VCARD_SUBVALUE, key, value);
}

#endif

static void
sig_vcard(XMPP_SERVER_REC *server, const char *jid, GHashTable *ht)
{
	XMPP_ROSTER_USER_REC *user;
	struct vcard_user_data ud;
	char *name;

	user = rosters_find_user(server->roster, jid, NULL, NULL);
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
fe_vcard_init(void)
{
	signal_add("xmpp vcard", sig_vcard);
}

void
fe_vcard_deinit(void)
{   
	signal_remove("xmpp vcard", sig_vcard);
}
