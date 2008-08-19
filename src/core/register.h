/* $Id$ */

#ifndef __REGISTER_H
#define __REGISTER_H

__BEGIN_DECLS
void	xmpp_register(const char *, int, const gboolean, const char *,
	    const char *, const char *);

void	register_init(void);
void	register_deinit(void);
__END_DECLS

#endif
