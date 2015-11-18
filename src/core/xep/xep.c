/*
 * Copyright (C) 2007,2008,2009 Colin DIDIER
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

#include "carbons.h"
#include "chatstates.h"
#include "composing.h"
#include "delay.h"
#include "disco.h"
#include "muc.h"
#include "oob.h"
#include "ping.h"
#include "registration.h"
#include "vcard.h"
#include "version.h"

void
xep_init(void)
{
	disco_init(); /* init sevice discovery first */
	carbons_init();
	chatstates_init();
	composing_init();
	delay_init();
	muc_init();
	oob_init();
	ping_init();
	registration_init();
	vcard_init();
	version_init();
}

void
xep_deinit(void)
{
	disco_deinit();
	carbons_deinit();
	chatstates_deinit();
	composing_deinit();
	delay_deinit();
	muc_deinit();
	oob_deinit();
	ping_deinit();
	registration_deinit();
	vcard_deinit();
	version_deinit();
}
