/* $Id$ */

#ifndef __DISCO_H
#define __DISCO_H

__BEGIN_DECLS
void	xmpp_add_feature(XMPP_SERVER_REC *, const char *);

void	disco_init(void);
void	disco_deinit(void);
__END_DECLS

#endif
