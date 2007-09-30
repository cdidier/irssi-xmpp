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
#include "loudmouth/loudmouth.h"

GSList *
lm_message_node_find_childs(LmMessageNode *node, const char *child_name)
{
	LmMessageNode *l;
	GSList *list = NULL;

	g_return_val_if_fail(node != NULL, NULL);
	g_return_val_if_fail(child_name != NULL, NULL);

	for (l = node->children; l; l = l->next) {
		if (strcmp(l->name, child_name) == 0)
			list = g_slist_prepend(list, l);
	}

	return list;
}

gboolean
lm_message_nodes_attribute_found(GSList *list, const char *name,
     const char *value, LmMessageNode **node)
{
	LmMessageNode *l;
	GSList *tmp;
	const char *attribute_value;

	g_return_val_if_fail(list != NULL, FALSE);
	g_return_val_if_fail(name != NULL, FALSE);
	g_return_val_if_fail(value != NULL, FALSE);

	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		l = tmp->data;
		
		attribute_value = lm_message_node_get_attribute(l, name);
		if (attribute_value != NULL &&
		    strcmp(attribute_value, value) == 0) {

			if (&*node != NULL)
				*node = l;

			return TRUE;
		}

	}

	return FALSE;
}
