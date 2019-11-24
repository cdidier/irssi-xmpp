#ifndef __TOOL_DATALIST_H
#define __TOOL_DATALIST_H

#include "xmpp-servers.h"

typedef struct datalist_rec {
	XMPP_SERVER_REC	*server;
	char		*jid;
	void		*data;
} DATALIST_REC;

typedef struct datalist_first {
	GSList *list;
	void (*freedata_func)(DATALIST_REC *);
} DATALIST;

#ifdef __cplusplus
extern "C" {
#endif
DATALIST	*datalist_new(void (*)(DATALIST_REC *));
void		 datalist_destroy(DATALIST *);
DATALIST_REC	*datalist_find(DATALIST *, XMPP_SERVER_REC *, const char *);
DATALIST_REC	*datalist_add(DATALIST *, XMPP_SERVER_REC *, const char *,
		     void *);
void		 datalist_free(DATALIST *, DATALIST_REC *);
void		 datalist_remove(DATALIST *, XMPP_SERVER_REC *, const char *);
void		 datalist_cleanup(DATALIST *, XMPP_SERVER_REC *);
#ifdef __cplusplus
}
#endif

#endif
