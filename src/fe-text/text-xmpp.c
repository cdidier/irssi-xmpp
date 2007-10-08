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
#include "modules.h"

#include "text-xmpp-composing.h"
#include "text-xmpp-nick.h"

void
text_xmpp_init(void)
{
	text_xmpp_composing_init();
/*	text_xmpp_nick_init();*/

	module_register("xmpp", "text");
}

void
text_xmpp_deinit(void)
{
	text_xmpp_composing_deinit();
/*	text_xmpp_nick_deinit();*/
}
