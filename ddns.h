/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: ddns.h 295 2013-06-18 06:32:24Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2008-03-26.
 *
 *	'ddns' is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation; either version 3 of the License,
 *	or (at your option) any later version.
 *
 *	'ddns' is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with 'ddns'; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/*
 *  Basic interface for accessing DDNS server.
 */

#ifndef _INC_DDNS
#define _INC_DDNS

#ifdef WIN32
#	define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdio.h>			/* FILE    */
#include <stdarg.h>			/* va_list */
#include "ddns_socket.h"
#include "ddns_sync.h"
#include "ddns_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DDNS_UNUSED(var)	((void)(var))
	
#define ddns_DL2B(val)	ddns_DB2L(val)
#define ddns_DB2L(val) (	(((val) & 0x000000FF) << 24) \
						|	(((val) & 0x0000FF00) << 8) \
						|	(((val) & 0x00FF0000) >> 8) \
						|	(((val) & 0xFF000000) >> 24) \
						)

#if defined(__BIG_ENDIAN__) && __BIG_ENDIAN__
#	define ddns_DL2N(val)	ddns_DL2B(val)
#	define ddns_DB2N(val)	(val)
#	define ddns_DN2L(val)	ddns_DB2L(val)
#	define ddns_DN2B(val)	(val)
#else
#	define ddns_DL2N(val)	(val)
#	define ddns_DB2N(val)	ddns_DB2L(val)
#	define ddns_DN2L(val)	(val)
#	define ddns_DN2B(val)	ddns_DL2B(val)
#endif

#ifdef __cplusplus
#define DDNS_BEGIN_INTERFACE_(ifname)			class ifname {
#define DDNS_BEGIN_INTERFACE(ifname, ifbase)	class ifname : public ifbase {
#define DDNS_DECLARE_METHOD(ret, name)				virtual ret name
#define DDNS_END_INTERFACE(ifname)				};
#else
#define DDNS_BEGIN_INTERFACE_(ifname)			typedef struct __##ifname ifname;\
												struct __##ifname {
#define DDNS_BEGIN_INTERFACE(ifname, ifbase)	struct ifname { \
													struct ifbase __ifbase;
#define DDNS_DECLARE_METHOD(ret, name)				ret (*name)
#define DDNS_END_INTERFACE(ifname)				};
#endif

enum ddns_protocol
{
	proto_unknown		= 0,
	proto_peanuthull	= 1,
	proto_dyndns		= 2,
	proto_dnspod		= 3
};

enum ddns_verbose
{
	verbose_normal		= 0,
	verbose_verbose		= 1,
	verbose_quiet		= 2
};

enum ddns_msg_type
{
	msg_type_debug		= 0,
	msg_type_info		= 1,
	msg_type_warning	= 2,
	msg_type_error		= 3
};

struct ddns_server
{
	char				domain[128];
	unsigned short		port;
	struct ddns_server	*next;
};

struct ddns_context
{
	ddns_socket				socket;			/* socket to DDNS server          */
	char					username[32];	/* DDNS account name              */
	char					password[32];	/* password the the DDNS account  */
	int						timeout;		/* connection timeout in seconds  */
	int						interval;		/* keep-alive interval in seconds */
	int						auto_restart;	/* if auto-restart at error       */
	int						exit_signal;	/* signal to quit                 */
	int						log_status;		/* see [ddns_vprintf]             */
	enum ddns_protocol		protocol;		/* DDNS protocol                  */
	enum ddns_verbose		verbose_mode;	/* verbose mode                   */
	struct ddns_server		*server;		/* available DDNS server list     */
	struct ddns_server		*domain;		/* list of domain names           */
	struct sockaddr_in		active_server;	/* address of the active server   */
	struct ddns_sync_object	sync_object;	/* sync object of the context     */
	void					*extra_data;	/* protocol specific data         */
	FILE					*stream_out;	/* stream to output log           */
	FILE					*stream_err;	/* stream to output error log     */
};

DDNS_BEGIN_INTERFACE_(ddns_interface)

	/**
	 *	Initialize the DDNS context for target protocol.
	 */
	DDNS_DECLARE_METHOD(ddns_error, initialize)(
		struct ddns_context	*	context
		);

	/**
	 *	Determine if IP address is changed.
	 */
	DDNS_DECLARE_METHOD(ddns_error, is_ip_changed)(
		struct ddns_context	*	context
		);

	/**
	 *	Get current ip address.
	 *
	 *	@param[out]	buffer	: buffer to save the ip address.
	 *	@param[in]	length	: size of the buffer in characters.
	 *
	 *	NOTE:
	 *		This function is optional, return [DDNS_ERROR_NOTIML] if it's not
	 *		implemented.
	 */
	DDNS_DECLARE_METHOD(ddns_error, get_ip_address)(
		struct ddns_context *	context,
		char				*	buffer,
		size_t					length
		);

	/**
	 *	Execute update
	 */
	DDNS_DECLARE_METHOD(ddns_error, do_update)(
		struct ddns_context	*	context
		);

	/**
	 *	Finalize the DDNS context when exiting.
	 */
	DDNS_DECLARE_METHOD(ddns_error, finalize)(
		struct ddns_context	*	context
		);

	/**
	 *	Destroy the DDNS interface.
	 */
	DDNS_DECLARE_METHOD(ddns_error, destroy)(
		ddns_interface * interface
		);

DDNS_END_INTERFACE(ddns_interface)


/**
 * Get version of the program
 */
const char * ddns_getver();


/**
 *	Initialize a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context to be initialized.
 */
void ddns_initcontext(struct ddns_context *context);


/**
 *	Execute requested operation in a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 *
 *	@return	DDNS_ERROR_SUCCESS if user request to quit, otherwise an error
 *			code will be returned.
 */
ddns_error ddns_execute(struct ddns_context * context);


/**
 *	Free all resources associated with the DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context to be freed.
 */
void ddns_clearcontext(struct ddns_context *context);


/**
 *	Create an extra data block in a DDNS context.
 *
 *	@note	If the context already has a extra data block, the previous one will
 *			be freed first.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 *	@param[in]	size		: size of the extra data block in bytes.
 *
 *	@return	If successful, it will return 1. Otherwise, 0 will be returned.
 */
int ddns_create_extra_param(struct ddns_context * context, size_t size);


/**
 *	Remove extra data block from a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 */
void ddns_remove_extra_param(struct ddns_context * context);


/**
 *	Add a domain name to the DDNS context.
 *
 *	@param[in/out]	context	: pointer to the DDNS context.
 *	@param[in]		domain	: pointer to the domain to be added.
 *
 *	@return	If successful, it will return 1. Otherwise, zero will be returned.
 */
int ddns_adddomain(
	struct ddns_context *context,
	const struct ddns_server *domain
	);


/**
 *	Remove a domain name from the DDNS context.
 *
 *	@param[in/out]	context	: pointer to the DDNS context.
 *	@param[in]		domain	: pointer to the domain to be added.
 *
 *	@return	If successful, it will return 1. Otherwise, zero will be returned.
 */
int ddns_remove_domain(
	struct ddns_context			*	context,
	const struct ddns_server	*	domain
	);


/**
 *	Add a server to the DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 *	@param[in]	server		: pointer to the server to be added.
 *
 *	@return	If successful, it will return 1. Otherwise, zero will be returned.
 */
int ddns_addserver(
	struct ddns_context *context,
	const struct ddns_server *server
	);


/**
 *	Connect to DDNS server.
 *
 *	@param[in]	context		: pointer to a DDNS context.
 *
 *	@return	If successful, it will return the connected socket. Otherwise,
 *			DDNS_INVALID_SOCKET will be returned.
 */
ddns_socket ddns_connect(struct ddns_context *context);


/**
 *	Show console messages for different message verbose mode.
 *		- ddns_vprintf	: show it unconditionally
 *		- ddns_printf	: show it unconditionally
 *		- ddns_printf_q	: show it in quiet mode
 *		- ddns_printf_n	: show it in normal mode
 *		- ddns_printf_v	: show it in verbose mode
 *
 *	@param[in]	context		: pointer to a DDNS context.
 *	@param[in]	format		: message format string, compatible with 'printf'.
 */
void ddns_printf_q(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...);
void ddns_printf_n(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...);
void ddns_printf_v(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...);

/**
 *	Output a formatted text message, to either stdout of log file.
 *
 *	@param[in]	context		: pointer to a DDNS context.
 *	@param[in]	type		: type of the text message.
 *	@param[in]	format		: message format string, compatible with 'printf'.
 *	@param]in]	args		: arguments to format text message.
 */
void ddns_msg(
	struct ddns_context	*	context,
	enum ddns_msg_type		type,
	const char			*	format,
	...
	);
void ddns_vmsg(
	struct ddns_context	*	context,
	enum ddns_msg_type		type,
	const char			*	format,
	va_list					args
	);

/**
 *	Wait until next update.
 *
 *	@param[in]	context	: the DDNS context.
 *
 *	@return		Return non-zero if the client should keep the domains on-line,
 *				otherwise return 0.
 */
int ddns_wait(struct ddns_context * context);


/**
 *	Compare 2 strings without regard to case.
 *
 *	@note		It's a replacement for [strcasecmp].
 *
 *	@param[in]	: the first string to be compared
 *	@param[in]	: the second string to be compared.
 *
 *	@return		Return negative value if [str1] less than [str2].
 *				Return 0 if [str1] identical to [str2].
 *				Return positive value if [str1]	greater than [str2].
 */
int ddns_strcasecmp(const char* str1, const char* str2);


/**
 *	Compare characters of two strings without regard to case.
 *
 *	@note		It's a replacement for [strncasecmp].
 *
 *	@param[in]	: the first string to be compared
 *	@param[in]	: the second string to be compared.
 *	@param[in]	: number of characters to compare.
 *
 *	@return		Return negative value if [str1] less than [str2].
 *				Return 0 if [str1] identical to [str2].
 *				Return positive value if [str1]	greater than [str2].
 */
int ddns_strncasecmp(const char* str1, const char* str2, size_t count);


/**
 *	Search a string in another string, ignoring case in both string.
 *
 *	@note		It's a replacement for [strcasestr].
 *
 *	@param[in]	: the string to be searched.
 *	@param[in]	: the string we're searching for.
 *
 *	@return		Return a pointer to the first occurrence of [str2] in [str1], or
 *				NULL if [str2] does not appear in [str1]. If [str2] points to a
 *				string of zero length, the function returns [str1].
 */
const char * ddns_strcasestr(const char* str1, const char* str2);


/**
 *	Convert error code to human readable string.
 *
 *	@param[in]	error_cide	: the error code.
 *
 *	@return		Pointer to the statically allocated human readable string.
 */
const char * ddns_err2str(ddns_error error_code);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* !defined(_INC_DDNS) */
