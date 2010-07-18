/* $Id$ */

#ifndef __REGISTRATION_H
#define __REGISTRATION_H

enum {
	REGISTRATION_ERROR_UNAUTHORIZED		= 401,
	REGISTRATION_ERROR_UNAUTHORIZED_REG	= 407,
	REGISTRATION_ERROR_UNIMPLEMENTED	= 501,
	REGISTRATION_ERROR_UNAVAILABLE		= 503,
	REGISTRATION_ERROR_CONFLICT		= 409,
	REGISTRATION_ERROR_TIMEOUT		= 408,
	REGISTRATION_ERROR_TIMEOUT_SERVER	= 504,
	REGISTRATION_ERROR_CONNECTION		= -3,
	REGISTRATION_ERROR_INFO			= -2,
	REGISTRATION_ERROR_UNKNOWN		= -1,
};

__BEGIN_DECLS
void registration_init(void);
void registration_deinit(void);
__END_DECLS

#endif
