/* $Id$ */

#ifndef __MUC_NICKLIST_H
#define __MUC_NICKLIST_H

#include "nicklist.h"
#include "muc.h"

/* Returns XMPP_NICK_REC if it's XMPP channel, NULL if it isn't. */
#define XMPP_NICK(nick) 					\
	PROTO_CHECK_CAST(NICK(nick), XMPP_NICK_REC, chat_type, "XMPP")

#define IS_XMPP_NICK(nick) 					\
	(XMPP_NICK(nick) ? TRUE : FALSE)

#define xmpp_nicklist_find(channel, name) 			\
	XMPP_NICK(nicklist_find(CHANNEL(channel), name))

struct _XMPP_NICK_REC {
	#include "nick-rec.h"

	int 	 show;
	char 	*status;

	int	 affiliation;
	int 	 role;
};

enum {
	XMPP_NICKLIST_AFFILIATION_NONE,
	XMPP_NICKLIST_AFFILIATION_OWNER,
	XMPP_NICKLIST_AFFILIATION_ADMIN,
	XMPP_NICKLIST_AFFILIATION_MEMBER,
	XMPP_NICKLIST_AFFILIATION_OUTCAST
};
extern const char *xmpp_nicklist_affiliation[];

enum {
	XMPP_NICKLIST_ROLE_NONE,
	XMPP_NICKLIST_ROLE_MODERATOR,
	XMPP_NICKLIST_ROLE_PARTICIPANT,
	XMPP_NICKLIST_ROLE_VISITOR
};
extern const char *xmpp_nicklist_role[];

__BEGIN_DECLS
XMPP_NICK_REC	*xmpp_nicklist_insert(MUC_REC *, const char *, const char *);
void		 xmpp_nicklist_rename(MUC_REC *, XMPP_NICK_REC *, const char *,
		     const char *);
int		 xmpp_nicklist_get_affiliation(const char *);
int		 xmpp_nicklist_get_role(const char *);
gboolean	 xmpp_nicklist_modes_changed(XMPP_NICK_REC *, int, int);
void		 xmpp_nicklist_set_modes(XMPP_NICK_REC *, int, int);
void		 xmpp_nicklist_set_presence(XMPP_NICK_REC *, int,
		     const char *);

void muc_nicklist_init(void);
void muc_nicklist_deinit(void);
__END_DECLS

#endif
