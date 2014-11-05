#ifndef __MUC_ROLE_H
#define __MUC_ROLE_H

enum {
	XMPP_ROLE_NONE,
	XMPP_ROLE_MODERATOR,
	XMPP_ROLE_PARTICIPANT,
	XMPP_ROLE_VISITOR
};
extern const char *xmpp_role[];

__BEGIN_DECLS
int		 xmpp_nicklist_get_role(const char *);
__END_DECLS

#endif
