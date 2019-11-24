#ifndef __DISCO_H
#define __DISCO_H

#ifdef __cplusplus
extern "C" {
#endif
void		disco_add_feature(char *);
gboolean	disco_have_feature(GSList *, const char *);
void		disco_request(XMPP_SERVER_REC *, const char *);

void disco_init(void);
void disco_deinit(void);
#ifdef __cplusplus
}
#endif

#define XMLNS "xmlns"

#endif
