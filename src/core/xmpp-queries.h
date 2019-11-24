#ifndef __XMPP_QUERIES_H
#define __XMPP_QUERIES_H

#include "queries.h"
#include "xmpp-servers.h"

/* Returns XMPP_QUERY_REC if it's XMPP query, NULL if it isn't. */
#define XMPP_QUERY(query)						\
	PROTO_CHECK_CAST(QUERY(query), XMPP_QUERY_REC, chat_type, "XMPP")

#define IS_XMPP_QUERY(query)						\
	(XMPP_QUERY(query) ? TRUE : FALSE)

#define xmpp_query_find(server, name)					\
	XMPP_QUERY(query_find(SERVER(server), name))

#define STRUCT_SERVER_REC XMPP_SERVER_REC
struct _XMPP_QUERY_REC {
	#include "query-rec.h"

	time_t		composing_time;
	gboolean	composing_visible;
};

#ifdef __cplusplus
extern "C" {
#endif
QUERY_REC	*xmpp_query_create(const char *, const char *, int);
#ifdef __cplusplus
}
#endif

#endif
