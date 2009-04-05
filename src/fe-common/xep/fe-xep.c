/*
 * $Id$
 *
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

#include "fe-composing.h"
#include "fe-delay.h"
#include "fe-muc.h"
#include "fe-ping.h"
#include "fe-registration.h"
#include "fe-vcard.h"
#include "fe-version.h"

void
fe_xep_init(void)
{
	fe_composing_init();
	fe_delay_init();
	fe_muc_init();
	fe_ping_init();
	fe_registration_init();
	fe_vcard_init();
	fe_version_init();
}

void
fe_xep_deinit(void)
{
	fe_composing_deinit();
	fe_delay_deinit();
	fe_muc_deinit();
	fe_ping_deinit();
	fe_registration_deinit();
	fe_vcard_deinit();
	fe_version_deinit();
}
