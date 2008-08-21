/*
 * $Id$
 *
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

#include "datalist.h"

DATALIST_REC *
datalist_find(DATALIST *dl, XMPP_SERVER_REC *server, const char *jid)
{
	GSList *tmp;
	DATALIST_REC *rec;

	for (tmp = dl->list; tmp != NULL; tmp = tmp->next) {
		rec = tmp->data;
		if (rec->server == server && strcmp(rec->jid, jid) == 0)
				return rec;
	}
	return NULL;
}

DATALIST_REC *
datalist_add(DATALIST *dl, XMPP_SERVER_REC *server, const char *jid,
    void *data)
{
	DATALIST_REC *rec;

	if ((rec = datalist_find(dl, server, jid)) != NULL) {
		dl->freedata_func(rec);
		rec->data = data;
	} else {
		rec = g_new0(DATALIST_REC, 1);
		rec->server = server;
		rec->jid = g_strdup(jid);
		rec->data = data;
		dl->list = g_slist_prepend(dl->list, rec);
	}
	return rec;
}

void
datalist_free(DATALIST *dl, DATALIST_REC *rec)
{
	dl->list = g_slist_remove(dl->list, rec);
	g_free(rec->jid);
	dl->freedata_func(rec);
	g_free(rec);
}

void
datalist_remove(DATALIST *dl, XMPP_SERVER_REC *server, const char *jid)
{
	DATALIST_REC *rec;

	if ((rec = datalist_find(dl, server, jid)) != NULL)
		datalist_free(dl, rec);
}

void
datalist_cleanup(DATALIST *dl, XMPP_SERVER_REC *server)
{
	GSList *tmp, *next;
	DATALIST_REC *rec;

	for (tmp = dl->list; tmp != NULL; tmp = next) {
		next = tmp->next;
		rec = tmp->data;
		if (server == NULL || rec->server == server)
			datalist_free(dl, rec);
	}
}

static void
dummy_freedata_func(DATALIST_REC *rec)
{
}

DATALIST *
datalist_new(void (*freedata_func)(DATALIST_REC *))
{
	DATALIST *dl;

	dl = g_new0(DATALIST, 1);
	dl->list = NULL;
	dl->freedata_func = freedata_func == NULL ?
	    dummy_freedata_func : freedata_func;
	return dl;
}

void
datalist_destroy(DATALIST *dl)
{
	datalist_cleanup(dl, NULL);
	g_free(dl);
}
