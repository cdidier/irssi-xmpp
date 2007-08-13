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
#include "module-formats.h"
#include "settings.h"
#include "signals.h"
#include "levels.h"

#include "xmpp.h"
#include "xmpp-servers.h"
#include "xmpp-rosters.h"

#include "printtext.h"
#include "themes.h"

static void
event_roster_group(XMPP_SERVER_REC *server, const char *group)
{
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_ROSTER_GROUP, group ? group
        : settings_get_str("roster_default_group"));
}

static void
event_roster_nick(XMPP_SERVER_REC *server, const XmppRosterUser *user)
{
    GSList *ressource_list;
    XmppRosterRessource *ressource;
    gchar *name;
    gchar *first_show = NULL, *show;
    gchar *ressource_str = NULL, *str;

    /* offline user ? */
    if (!user->ressources)
        first_show = (gchar *) xmpp_presence_show[XMPP_PRESENCE_UNAVAILABLE];

    ressource_list = user->ressources;
    while (ressource_list) {
        ressource = (XmppRosterRessource *) ressource_list->data;
        
        show = (gchar *) xmpp_presence_show[ressource->show];
        
        if (!first_show) 
            first_show = show;
        
        if(ressource->show == XMPP_PRESENCE_AVAILABLE)
            show = NULL;
        
        if (ressource->name) {
            str = g_strdup_printf("%s[%s%s%s%s(%d)%s%s]",
                ressource_str ? ressource_str : "",
                show ? "(" : "", show ? show : "", show ? ")" : "",
                ressource->name,
                ressource->priority,
                ressource->status ? ": " : "",
                ressource->status ? ressource->status : "");
            
            g_free(ressource_str);
            ressource_str = str;
        }
        
        ressource_list = g_slist_next(ressource_list);
    }

    if (user->error)
        first_show = (gchar *) xmpp_presence_show[XMPP_PRESENCE_ERROR];

    if (user->subscription != XMPP_SUBSCRIPTION_BOTH) {
        str = g_strdup_printf("%s%s(subscription: %s)",
            ressource_str ? ressource_str : "",
            ressource_str ? " " : "",
            xmpp_subscription[user->subscription]);
        
        g_free(ressource_str);
        ressource_str = str;
    }

    if (user->name) {
        name = user->name;

        str = g_strdup_printf("(%s) %s",
            user->jid,
            ressource_str ? ressource_str : "");
        g_free(ressource_str);
        ressource_str = str;

    } else
        name = user->jid;
    
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_ROSTER_NICK, first_show, name, ressource_str);
    
    g_free(ressource_str);
}

static void
event_begin_of_roster(XMPP_SERVER_REC *server)
{   
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_BEGIN_OF_ROSTER);
}

static void
event_end_of_roster(XMPP_SERVER_REC *server)
{   
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_END_OF_ROSTER);
}

static void
event_not_in_roster(XMPP_SERVER_REC *server, char *jid)
{   
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CLIENTERROR,
        XMPPTXT_NOT_IN_ROSTER, jid);
}

static void
event_subscribe(XMPP_SERVER_REC *server, char *jid, char *status)
{
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_SUBSCRIBE, jid, status);
}

static void
event_subscribed(XMPP_SERVER_REC *server, char *jid)
{
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_SUBSCRIBED, jid);
}

static void
event_unsubscribe(XMPP_SERVER_REC *server, char *jid)
{
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_UNSUBSCRIBE, jid);
}

static void
event_unsubscribed(XMPP_SERVER_REC *server, char *jid)
{
    printformat_module(MODULE_NAME, server, NULL, MSGLEVEL_CRAP,
        XMPPTXT_UNSUBSCRIBED, jid);
}

void
fe_xmpp_roster_init(void)
{
    signal_add("xmpp event roster group", (SIGNAL_FUNC) event_roster_group);
    signal_add("xmpp event roster nick", (SIGNAL_FUNC) event_roster_nick);
    signal_add("xmpp event begin of roster",
        (SIGNAL_FUNC) event_begin_of_roster);
    signal_add("xmpp event end of roster", (SIGNAL_FUNC) event_end_of_roster);
    signal_add("xmpp event not in roster", (SIGNAL_FUNC) event_not_in_roster);
    signal_add("xmpp event subscribe", (SIGNAL_FUNC) event_subscribe);
    signal_add("xmpp event subscribed", (SIGNAL_FUNC) event_subscribed);
    signal_add("xmpp event unsubscribe", (SIGNAL_FUNC) event_unsubscribe);
    signal_add("xmpp event unsubscribed", (SIGNAL_FUNC) event_unsubscribed);
}

void
fe_xmpp_roster_deinit(void)
{
    signal_remove("xmpp event roster group", (SIGNAL_FUNC) event_roster_group);
    signal_remove("xmpp event roster nick", (SIGNAL_FUNC) event_roster_nick);
    signal_remove("xmpp event begin of roster",
        (SIGNAL_FUNC) event_begin_of_roster);
    signal_remove("xmpp event end of roster",
        (SIGNAL_FUNC) event_end_of_roster);
    signal_remove("xmpp event not in roster",
        (SIGNAL_FUNC) event_not_in_roster);
    signal_remove("xmpp event subscribe", (SIGNAL_FUNC) event_subscribe);
    signal_remove("xmpp event subscribed", (SIGNAL_FUNC) event_subscribed);
    signal_remove("xmpp event unsubscribe", (SIGNAL_FUNC) event_unsubscribe);
    signal_remove("xmpp event unsubscribed", (SIGNAL_FUNC) event_unsubscribed);
}
