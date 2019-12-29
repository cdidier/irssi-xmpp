#ifndef __XMPP_COMMANDS_H
#define __XMPP_COMMANDS_H

#include <irssi/src/core/commands.h>

enum {
	XMPP_COMMAND_AWAY,
	XMPP_COMMAND_QUOTE,
	XMPP_COMMAND_ROSTER,
	XMPP_COMMAND_WHOIS,
	XMPP_COMMAND_PRESENCE
};
extern const char *xmpp_commands[];

enum {
	XMPP_COMMAND_ROSTER_PARAM_FULL,
	XMPP_COMMAND_ROSTER_PARAM_ADD,
	XMPP_COMMAND_ROSTER_PARAM_REMOVE,
	XMPP_COMMAND_ROSTER_PARAM_NAME,
	XMPP_COMMAND_ROSTER_PARAM_GROUP,
};
extern const char *xmpp_command_roster[];

enum {
	XMPP_COMMAND_ROSTER_PARAM_ACCEPT,
	XMPP_COMMAND_ROSTER_PARAM_DENY,
	XMPP_COMMAND_ROSTER_PARAM_SUBSCRIBE,
	XMPP_COMMAND_ROSTER_PARAM_UNSUBSCRIBE,
};
extern const char *xmpp_command_presence[];

#define command_bind_xmpp(cmd, section, signal)				\
	command_bind_proto(cmd, XMPP_PROTOCOL, section, signal)		

#define command_bind_xmpp_first(cmd, section, signal)			\
	command_bind_proto_first(cmd, XMPP_PROTOCOL, section, signal)

#define command_bind_xmpp_last(cmd, section, signal)			\
	command_bind_proto_last(cmd, XMPP_PROTOCOL, section, signal)

/* Simply returns if server isn't for XMPP protocol. Prints ERR_NOT_CONNECTED
 * error if there's no server or server isn't connected yet */
#define CMD_XMPP_SERVER(server)						\
	G_STMT_START {							\
		if (((server) != NULL) && !IS_XMPP_SERVER(server)) {	\
			 return;					\
		}							\
		if (((server) == NULL) || !(server)->connected)	{	\
			cmd_return_error(CMDERR_NOT_CONNECTED);		\
		}							\
	} G_STMT_END

__BEGIN_DECLS
char	*xmpp_get_dest(const char *, XMPP_SERVER_REC *, WI_ITEM_REC *);

void	xmpp_commands_init(void);
void	xmpp_commands_deinit(void);
__END_DECLS

#endif
