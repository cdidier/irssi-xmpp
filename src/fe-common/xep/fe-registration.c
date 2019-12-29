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
#include <irssi/src/core/ignore.h>
#include <irssi/src/core/levels.h>
#include "module-formats.h"
#include <irssi/src/fe-common/core/printtext.h>
#include <irssi/src/core/signals.h>
#include <irssi/src/fe-common/core/module-formats.h>
#include <irssi/src/fe-common/irc/module-formats.h>

#include "xmpp-servers.h"
#include "xep/registration.h"

static void
sig_failed(const char *username, const char *domain, gpointer error)
{
	char *reason, *str = NULL;

	switch(GPOINTER_TO_INT(error)) {
	case REGISTRATION_ERROR_UNAUTHORIZED:
	case REGISTRATION_ERROR_UNAUTHORIZED_REG:
		reason = "Registration unauthorized";
		break;
	case REGISTRATION_ERROR_UNIMPLEMENTED:
	case REGISTRATION_ERROR_UNAVAILABLE:
		reason = "Service unavailable";
		break;
	case REGISTRATION_ERROR_CONFLICT:
		reason = "Account already exists";
		break;
	case REGISTRATION_ERROR_TIMEOUT:
	case REGISTRATION_ERROR_TIMEOUT_SERVER:
		reason = "Connection times out";
		break;
	case REGISTRATION_ERROR_CLOSED:
		reason = "Connection was closed";
		break;
	case REGISTRATION_ERROR_CONNECTION:
		reason = "Cannot open connection";
		break;
	case REGISTRATION_ERROR_INFO:
		reason = "Cannot send registration information";
		break;
	case REGISTRATION_ERROR_UNKNOWN:
		reason = "Cannot register account (unknown reason)";
		break;
	default:
		reason = str = g_strdup_printf("Cannot register account (%d)",
		    GPOINTER_TO_INT(error));
	}
	printformat_module(MODULE_NAME, NULL, NULL,
	    MSGLEVEL_CRAP, XMPPTXT_REGISTRATION_FAILED, username, domain,
	    reason);
	g_free(str);
}

static void
sig_succeed(const char *username, const char *domain)
{
	printformat_module(MODULE_NAME, NULL, NULL,
	    MSGLEVEL_CRAP, XMPPTXT_REGISTRATION_SUCCEED, username, domain);
}

static void
sig_started(const char *username, const char *domain)
{
	printformat_module(MODULE_NAME, NULL, NULL,
	    MSGLEVEL_CRAP, XMPPTXT_REGISTRATION_STARTED, username, domain);
}

void
fe_registration_init(void)
{
	signal_add("xmpp registration failed", sig_failed);
	signal_add("xmpp registration succeed", sig_succeed);
	signal_add("xmpp registration started", sig_started);
}

void
fe_registration_deinit(void)
{
	signal_remove("xmpp registration failed", sig_failed);
	signal_remove("xmpp registration succeed", sig_succeed);
	signal_remove("xmpp registration started", sig_started);
}
