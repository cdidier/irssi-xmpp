#ifndef __FE_XMPP_STATUS_H
#define __FE_XMPP_STATUS_H

extern const char *fe_xmpp_presence_show[];
extern const int   fe_xmpp_presence_show_format[];

__BEGIN_DECLS
char		*fe_xmpp_status_get_window_name(XMPP_SERVER_REC *);
WINDOW_REC	*fe_xmpp_status_get_window(XMPP_SERVER_REC *);

void fe_xmpp_status_init(void);
void fe_xmpp_status_deinit(void);
__END_DECLS

#endif
