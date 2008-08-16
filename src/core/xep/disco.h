/* $Id$ */

#ifndef __DISCO_H
#define __DISCO_H

__BEGIN_DECLS
void		xmpp_add_feature(char *);
gboolean	xmpp_have_feature(GSList *, const char *);

void	disco_init(void);
void	disco_deinit(void);
__END_DECLS

#endif
