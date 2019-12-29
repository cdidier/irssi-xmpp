#ifndef __XMPP_SERVERS_H
#define __XMPP_SERVERS_H

#include <irssi/src/core/chat-protocols.h>
#include <irssi/src/core/servers.h>

#include "loudmouth/loudmouth.h"
#include "loudmouth-tools.h"

#define XMPP_PROXY_HTTP "http"

/* returns XMPP_SERVER_REC if it's XMPP server, NULL if it isn't */
#define XMPP_SERVER(server)						\
	PROTO_CHECK_CAST(SERVER(server), XMPP_SERVER_REC, chat_type, "XMPP")

#define XMPP_SERVER_CONNECT(conn)					\
	PROTO_CHECK_CAST(SERVER_CONNECT(conn), XMPP_SERVER_CONNECT_REC,	\
	    chat_type, "XMPP")

#define IS_XMPP_SERVER(server)						\
	(XMPP_SERVER(server) ? TRUE : FALSE)

#define IS_XMPP_SERVER_CONNECT(conn)					\
	(XMPP_SERVER_CONNECT(conn) ? TRUE : FALSE) 

struct _XMPP_SERVER_CONNECT_REC {
	#include <irssi/src/core/server-connect-rec.h>

	GSList		*channels_list;
	int		 show;
	int		 priority;
	char		*real_jid;
	char		*prompted_password;
};

#define STRUCT_SERVER_CONNECT_REC XMPP_SERVER_CONNECT_REC
struct _XMPP_SERVER_REC {
	#include <irssi/src/core/server-rec.h>

	char		*jid;
	char		*user;
	char		*domain;
	char		*resource;

	int		 show;
	int		 priority;
	char		*ping_id;
	GSList		*server_features;
	GSList		*my_resources;
	GSList		*roster;

	int		 timeout_tag;
	LmConnection	*lmconn;
	GSList		*msg_handlers;
};

__BEGIN_DECLS
SERVER_REC	*xmpp_server_init_connect(SERVER_CONNECT_REC *);
void		 xmpp_server_connect(XMPP_SERVER_REC *);

void        	xmpp_servers_init(void);
void		xmpp_servers_deinit(void);
__END_DECLS

#endif
