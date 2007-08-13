#ifndef __XMPP_QUERIES_H
#define __XMPP_QUERIES_H

#include "queries.h"
#include "xmpp-servers.h"

/* Returns XMPP_QUERY_REC if it's XMPP query, NULL if it isn't. */
#define XMPP_QUERY(query)                                                      \
    PROTO_CHECK_CAST(QUERY(query), QUERY_REC, chat_type, "XMPP")

#define IS_XMPP_QUERY(query)                                                   \
    (XMPP_QUERY(query) ? TRUE : FALSE)

QUERY_REC *xmpp_query_create(const char *server_tag, const char *nick,
    int automatic);

#define xmpp_query_find(server, name)                                          \
    query_find(SERVER(server), name)

#endif
