/* $Id:*/

#ifndef __LOUDMOUTH_TOOLS_H
#define __LOUDMOUTH_TOOLS_H

__BEGIN_DECLS
GSList	*lm_message_node_find_childs(LmMessageNode *, const char *);
gboolean lm_message_nodes_attribute_found(GSList *, const char *, const char *,
	     LmMessageNode **);
__END_DECLS

#endif
