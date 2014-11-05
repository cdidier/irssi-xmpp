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

#include "muc-affiliation.h"

const char *xmpp_affiliation[] = {
	"none",
	"owner",
	"admin",
	"member",
	"outcast",
	NULL
};

int
xmpp_nicklist_get_affiliation(const char *affiliation)
{
	if (affiliation != NULL) {
		if (g_ascii_strcasecmp(affiliation,
		    xmpp_affiliation[XMPP_AFFILIATION_OWNER]) == 0)
			return XMPP_AFFILIATION_OWNER;
		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_affiliation[XMPP_AFFILIATION_ADMIN]) == 0)
			return XMPP_AFFILIATION_ADMIN;
		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_affiliation[XMPP_AFFILIATION_MEMBER]) == 0)
			return XMPP_AFFILIATION_MEMBER;
		else if (g_ascii_strcasecmp(affiliation,
		    xmpp_affiliation[XMPP_AFFILIATION_OUTCAST]) == 0)
			return XMPP_AFFILIATION_OUTCAST;
	}
	return XMPP_AFFILIATION_NONE;
}
