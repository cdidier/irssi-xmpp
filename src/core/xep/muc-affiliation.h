#ifndef __MUC_AFFILIATION_H
#define __MUC_AFFILIATION_H

enum {
	XMPP_AFFILIATION_NONE,
	XMPP_AFFILIATION_OWNER,
	XMPP_AFFILIATION_ADMIN,
	XMPP_AFFILIATION_MEMBER,
	XMPP_AFFILIATION_OUTCAST
};
extern const char *xmpp_affiliation[];

__BEGIN_DECLS
int		 xmpp_nicklist_get_affiliation(const char *);
__END_DECLS

#endif
