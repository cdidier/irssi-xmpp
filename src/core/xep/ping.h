#ifndef __PING_H
#define __PING_H

#ifdef __cplusplus
extern "C" {
#endif
void xmpp_ping_send(XMPP_SERVER_REC *, const char *);

void ping_init(void);
void ping_deinit(void);
#ifdef __cplusplus
}
#endif

#endif
