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
#include <irssi/src/fe-common/core/formats.h>
#include <irssi/src/core/signals.h>

static void
sig_strip_codes(const char *in, const char **out)
{
	if (out != NULL)
		*out = strip_codes(in);
}

void
xmpp_formats_init(void)
{
	signal_add("xmpp formats strip codes", sig_strip_codes);
}

void
xmpp_formats_deinit(void)
{
	signal_remove("xmpp formats strip codes", sig_strip_codes);
}
