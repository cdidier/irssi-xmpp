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

#include "module.h"
#include "settings.h"

#define XMPP_PRIORITY_MIN -128
#define XMPP_PRIORITY_MAX 127

static const char *utf8_charset = "UTF-8";

gboolean
get_local_charset(G_CONST_RETURN char **charset)
{
	G_CONST_RETURN char *local_charset;
	gboolean is_utf8 = FALSE;

	local_charset = settings_get_str("term_charset");
	if (local_charset == NULL)
		is_utf8 = g_get_charset(&local_charset);

	*charset = local_charset;

	return (!is_utf8 && g_strcasecmp(local_charset, utf8_charset) == 0) ?
	    TRUE : is_utf8;
}

char *
xmpp_recode_out(const char *str)
{
	G_CONST_RETURN char *local_charset;
	char *recoded;

	g_return_val_if_fail(str != NULL, NULL);

	recoded = !get_local_charset(&local_charset) ?
	    g_convert(str, -1, utf8_charset, local_charset, NULL, NULL, NULL) :
	    NULL;

	return (recoded != NULL) ? recoded : g_strdup(str);
}

char *
xmpp_recode_in(const char *str)
{
	G_CONST_RETURN char *local_charset;
	char *recoded = NULL;

	g_return_val_if_fail(str != NULL, NULL);

	recoded = !get_local_charset(&local_charset) ?
	    g_convert(str, -1, local_charset, utf8_charset, NULL, NULL, NULL) :
	    NULL;

	return (recoded != NULL) ? recoded : g_strdup(str);
}

char *
xmpp_jid_get_ressource(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '/');
	return (pos != NULL) ? g_strdup(pos + 1) : NULL;
}

char *
xmpp_jid_strip_ressource(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '/');
	return (pos != NULL) ?  g_strndup(jid, pos - jid) : g_strdup(jid);
}

char *
xmpp_jid_get_username(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '@');
	return (pos != NULL) ? g_strndup(jid, pos - jid) :
	    xmpp_jid_strip_ressource(jid);
}

gboolean
xmpp_jid_have_address(const char *jid)
{
        g_return_val_if_fail(jid != NULL, FALSE);

        return (g_utf8_strchr(jid, -1, '@') != NULL) ? TRUE : FALSE;
}

gboolean
xmpp_jid_have_ressource(const char *jid)
{
        g_return_val_if_fail(jid != NULL, FALSE);

        return (g_utf8_strchr(jid, -1, '/') != NULL) ? TRUE : FALSE;
}

gboolean
xmpp_priority_out_of_bound(const int priority)
{
        return (XMPP_PRIORITY_MIN <= priority
            && priority <= XMPP_PRIORITY_MAX) ? FALSE : TRUE;
}
