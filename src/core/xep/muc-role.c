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

#include <glib.h>

#include "muc-role.h"

const char *xmpp_role[] = {
	"none",
	"moderator",
	"participant",
	"visitor",
	NULL
};

int
xmpp_nicklist_get_role(const char *role)
{
	if (role != NULL) {
		if (g_ascii_strcasecmp(role,
		    xmpp_role[XMPP_ROLE_MODERATOR]) == 0)
			return XMPP_ROLE_MODERATOR;
		else if (g_ascii_strcasecmp(role,
		    xmpp_role[XMPP_ROLE_PARTICIPANT]) == 0)
			return XMPP_ROLE_PARTICIPANT;
		else if (g_ascii_strcasecmp(role,
		    xmpp_role[XMPP_ROLE_VISITOR]) == 0)
			return XMPP_ROLE_VISITOR;
	}
	return XMPP_ROLE_NONE;
}
