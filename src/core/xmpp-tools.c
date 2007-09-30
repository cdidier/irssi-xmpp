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
#include "settings.h"

#define XMPP_PRIORITY_MIN -128
#define XMPP_PRIORITY_MAX 127

static const char *utf8_charset = "UTF-8";

static gboolean
get_local_charset(G_CONST_RETURN char **charset)
{
	G_CONST_RETURN char *local_charset;
	gboolean is_utf8 = FALSE;

	local_charset = settings_get_str("term_charset");
	is_utf8 = (local_charset == NULL) ? g_get_charset(&local_charset)
	    : (g_ascii_strcasecmp(local_charset, utf8_charset) == 0);

	*charset = local_charset;
	return is_utf8;
}

char *
xmpp_recode_out(const char *str)
{
	G_CONST_RETURN char *local_charset;
	char *recoded;

	if (str == NULL)
		return NULL;

	if (*str == '\0')
		return g_strdup(str);

	recoded = !get_local_charset(&local_charset) ?
	    g_convert(str, -1, utf8_charset, local_charset, NULL, NULL, NULL)
	    : NULL;

	return (recoded != NULL) ? recoded : g_strdup(str);
}

char *
xmpp_recode_in(const char *str)
{
	G_CONST_RETURN char *local_charset;
	char *recoded;

	if (str == NULL)
		return NULL;

	if (*str == '\0')
		return g_strdup(str);

	recoded = !get_local_charset(&local_charset) ?
	    g_convert(str, -1, local_charset, utf8_charset, NULL, NULL, NULL)
	    : NULL;

	return (recoded != NULL) ? recoded : g_strdup(str);
}

char *
xmpp_extract_resource(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '/');
	return (pos != NULL) ? g_strdup(pos + 1) : NULL;
}

char *
xmpp_strip_resource(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '/');
	return (pos != NULL) ?  g_strndup(jid, pos - jid) : g_strdup(jid);
}

char *
xmpp_extract_user(const char *jid)
{
        char *pos;

        g_return_val_if_fail(jid != NULL, NULL);

        pos = g_utf8_strchr(jid, -1, '@');
	return (pos != NULL) ? g_strndup(jid, pos - jid) :
	    xmpp_strip_resource(jid);
}

char *
xmpp_extract_host(const char *jid)
{
	return NULL;
}

void
xmpp_jid_extract(char *jid, char **username, char **server,
    char **resource)
{
	g_return_if_fail(jid != NULL);

	*username = jid;

	*server = g_utf8_strchr(jid, -1, '@');
	if (*server != NULL) {
		**server = '\0';
		if (*(*server + 1) != '\0')
			(*server)++;
		else
			*server = NULL;

		*resource = g_utf8_strchr(*server, -1, '/');
		if (*resource != NULL) {
			**resource = '\0';
			if (*(*resource + 1) != '\0')
				(*resource)++;
			else
				*resource = NULL;
		}
	} else
		*resource = NULL;
}

gboolean
xmpp_jid_have_address(const char *jid)
{
	char *pos;

        g_return_val_if_fail(jid != NULL, FALSE);

	pos = g_utf8_strchr(jid, -1, '@');
	return (pos != NULL && *(pos+1) != '\0');
}

gboolean
xmpp_jid_have_resource(const char *jid)
{
	char *pos;

        g_return_val_if_fail(jid != NULL, FALSE);

	pos = g_utf8_strchr(jid, -1, '/');
        return (pos != NULL && *(pos+1) != '\0');
}

gboolean
xmpp_priority_out_of_bound(const int priority)
{
        return (XMPP_PRIORITY_MIN <= priority
            && priority <= XMPP_PRIORITY_MAX) ? FALSE : TRUE;
}

gboolean
xmpp_presence_changed(const int show, const int old_show, const char *status,
    const char *old_status, const int priority, const int old_priority)
{
	return (show != old_show)
	    || (status == NULL && old_status != NULL)
	    || (status != NULL && old_status == NULL)
	    || (status != NULL && old_status != NULL
	    && strcmp(status, old_status) != 0)
	    || (priority != old_priority);
}
