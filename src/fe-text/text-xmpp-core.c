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
#include <irssi/src/core/modules.h>

#include "xep/text-xep.h"

void
text_xmpp_init(void)
{
	text_xep_init();

	module_register("xmpp", "text");
}

void
text_xmpp_deinit(void)
{
	text_xep_deinit();
}

#ifdef IRSSI_ABI_VERSION
void
text_xmpp_abicheck(int * version)
{
	*version = IRSSI_ABI_VERSION;
}
#endif
