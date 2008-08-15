/* $Id$ */

#ifndef __FEATURES_H
#define __FEATURES_H

__BEGIN_DECLS
void	xmpp_add_feature(XMPP_SERVER_REC *, const char *);

void	features_init(void);
void	features_deinit(void);
__END_DECLS

#endif
