/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: ddns.c 300 2014-01-24 05:13:57Z kuang $
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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "ddns.h"
#include <stdio.h>		/* vprintf, fflush                       */
#include <stdlib.h>		/* malloc, free                          */
#include <string.h>		/* memset, memcpy, strlen                */
#ifndef WIN32
#	include <strings.h>	/* POSIX.1-2001: strcasecmp, strncasecmp */
#endif
#include "ddns_string.h"
#include "oraypeanut.h"
#include "dnspod.h"
#include "dyndns.h"

#ifdef TIME_WITH_SYS_TIME
#	include <sys/time.h>
#	include <time.h>	/* C89, POSIX.1-2001: time, localtime, strftime */
#else
#	ifdef HAVE_SYS_TIME_H
#		include <sys/time.h>
#	else
#		include <time.h>
#	endif
#endif

/* for XCode */
#ifdef HAVE_REPOSITORY_REVISION
#	include "ddns_version.h"
#endif


/**
 * Get version of the program
 */
const char * ddns_getver()
{
#ifdef REPOSITORY_REVISION
	static const char szRevision[] = REPOSITORY_REVISION;
#else
	static char szFullRevision[] = "$Revision: 300 $";
	const char* const szRevision = &(szFullRevision[11]);
	szFullRevision[sizeof(szFullRevision)-3] = 'M';
	szFullRevision[sizeof(szFullRevision)-2] = '\0';
#endif

	return szRevision;
}


/*
 *	Initialize a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context to be initialized.
 */
void ddns_initcontext(struct ddns_context * context)
{
	if ( NULL != context )
	{
		memset(context, 0, sizeof(*context));

		context->socket			= DDNS_INVALID_SOCKET;
		context->timeout		= 15;
		context->interval		= 60;
		context->auto_restart	= 60;
		context->protocol		= proto_unknown;
		context->verbose_mode	= verbose_normal;
		context->server			= NULL;
		context->domain			= NULL;
		context->extra_data		= NULL;

		/* Do NOT assign stdout/stderr to them, it will cause problem */
		context->stream_out		= NULL;
		context->stream_err		= NULL;

		ddns_sync_init(&(context->sync_object));
	}
}


/*
 *	Free all resources associated with the DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context to be freed.
 */
void ddns_clearcontext(struct ddns_context* context)
{
	struct ddns_server *svrlst = NULL;
	struct ddns_server *server = NULL;

	ddns_remove_extra_param(context);

	if ( DDNS_INVALID_SOCKET != context->socket )
	{
		ddns_socket_close(context->socket);
		context->socket = DDNS_INVALID_SOCKET;
	}

	for ( svrlst = context->domain; svrlst; svrlst = server )
	{
		server = svrlst->next;
		free(svrlst);
	}
	context->domain = NULL;

	for ( svrlst = context->server; svrlst; svrlst = server )
	{
		server = svrlst->next;
		free(svrlst);
	}
	context->server = NULL;

	if (NULL != context->stream_out)
	{
		fclose(context->stream_out);
		context->stream_out = NULL;
	}

	if (NULL != context->stream_err)
	{
		fclose(context->stream_err);
		context->stream_err = NULL;
	}

	ddns_sync_destroy(&(context->sync_object));
}


/**
 *	Execute requested operation in a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 */
ddns_error ddns_execute(struct ddns_context * context)
{
	ddns_interface * ddns = NULL;
	ddns_error error_code = DDNS_ERROR_SUCCESS;

	ddns_socket_init();

	switch( context->protocol )
	{
#ifndef DISABLE_PEANUTHULL
	case proto_peanuthull:
		ddns = peanuthull_interface_create();
		break;
#endif

#ifndef DISABLE_DNSPOD
	case proto_dnspod:
		ddns = dnspod_create_interface();
		break;
#endif

#ifndef DISABLE_DYNDNS
	case proto_dyndns:
		ddns = dyndns_create_interface();
		break;
#endif

	default:
		error_code = DDNS_FATAL_ERROR(DDNS_ERROR_INVALID_PROTO);
		ddns_msg(context, msg_type_error, "%s.\n", ddns_err2str(error_code));
		break;
	}

	while ( NULL != ddns )
	{
		ddns_printf_n(context, msg_type_info, "Initializing... ");
		error_code = ddns->initialize(context);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			ddns_printf_n(context, msg_type_info, "done.\n");
		}
		else
		{
			ddns_printf_n(context, msg_type_info, "failed.\n");
		}

		/**
		 *	Main loop: keep check if ip address at specified interval. If it's
		 *	changed, send update request to server.
		 */
		while ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( 0 == ddns_wait(context) )
			{
				break;
			}

			error_code = ddns->is_ip_changed(context);
			if ( DDNS_ERROR_NOCHG == error_code )
			{
				/* ip address not changed, skip updating */
				error_code = DDNS_ERROR_SUCCESS;
				continue;
			}

			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				char		ip_address[16];
				ddns_error	result = DDNS_ERROR_SUCCESS;

				result = ddns->get_ip_address(	context,
												ip_address,
												_countof(ip_address)
												);
				if ( DDNS_ERROR_SUCCESS == result )
				{
					ddns_printf_n(	context,
									msg_type_info,
									"IP address changed to \"%s\".\n",
									ip_address
									);
				}
			}

			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				ddns_printf_n(context, msg_type_info, "Updating DNS records... ");
				error_code = ddns->do_update(context);
				if ( DDNS_ERROR_SUCCESS == error_code )
				{
					ddns_printf_n(context, msg_type_info, "done.\n");
				}
				else
				{
					ddns_printf_n(context, msg_type_info, "failed.\n");
				}
			}
		}

		ddns->finalize(context);

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			/* stop at user's request */
			break;
		}
		else
		{
			int interval = context->interval;

			ddns_msg(context, msg_type_error, "%s.\n", ddns_err2str(error_code));
			if (DDNS_IS_FATAL_ERROR(error_code))
			{
				break;
			}

			context->interval = context->auto_restart;
			if ( (0 == context->auto_restart) || (0 == ddns_wait(context)) )
			{
				context->interval = interval;
				break;
			}
			context->interval = interval;
		}
	}

	if ( NULL != ddns )
	{
		ddns->destroy(ddns);
		ddns = NULL;
	}

	ddns_socket_uninit();

	return error_code;
}


/*
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
int ddns_create_extra_param(struct ddns_context * context, size_t size)
{
	ddns_remove_extra_param(context);

	context->extra_data = malloc(size);
	if ( NULL != context->extra_data )
	{
		memset(context->extra_data, 0, size);
	}

	return ( NULL == context->extra_data ? 0 : 1 );
}


/**
 *	Remove extra data block from a DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 */
void ddns_remove_extra_param(struct ddns_context * context)
{
	if ( NULL != context && NULL != context->extra_data )
	{
		free(context->extra_data);
		context->extra_data = NULL;
	}
}


/*
 *	Add a domain name to the DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 *	@param[in]	domain		: pointer to the domain to be added.
 *
 *	@return	If successful, it will return 1. Otherwise, zero will be returned.
 */
int ddns_adddomain(struct ddns_context* context, const struct ddns_server* domain)
{
	/* get the last server */
	if ( NULL == context->domain )
	{
		context->domain = malloc(sizeof(*domain));
		if ( NULL == context->domain )
		{
			return 0;
		}
		memcpy(context->domain, domain, sizeof(*domain));
	}
	else
	{
		struct ddns_server *svrlst;
		for ( svrlst = context->domain; svrlst->next; svrlst = svrlst->next );

		svrlst->next = malloc(sizeof(*domain));
		if ( NULL == svrlst->next )
		{
			return 0;
		}

		memcpy(svrlst->next, domain, sizeof(*domain));
	}

	return 1;
}


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
	)
{
	int						retval		= 0;

	if ( NULL != context && NULL != context->domain )
	{
		struct ddns_server	*	cur		= NULL;
		struct ddns_server	*	prev	= NULL;

		for ( cur = context->domain; NULL != cur; prev = cur, cur = cur->next)
		{
			if ( 0 != ddns_strcasecmp(cur->domain, domain->domain) )
			{
				continue;
			}
			if ( cur->port != domain->port )
			{
				continue;
			}

			retval = 1;
			if ( NULL == prev )
			{
				context->domain = cur->next;
			}
			else
			{
				prev->next = cur->next;
			}
			free(cur);
			break;
		}
	}

	return retval;
}


/*
 *	Add a server to the DDNS context.
 *
 *	@param[in]	context		: pointer to the DDNS context.
 *	@param[in]	server		: pointer to the server to be added.
 *
 *	@return	If successful, it will return 1. Otherwise, zero will be returned.
 */
int ddns_addserver(struct ddns_context* context, const struct ddns_server* server)
{
	/* get the last server */
	if ( NULL == context->server )
	{
		context->server = malloc(sizeof(*server));
		if ( NULL == context->server )
		{
			return 0;
		}
		memcpy(context->server, server, sizeof(*server));
	}
	else
	{
		struct ddns_server *svrlst;
		for ( svrlst = context->server; svrlst->next; svrlst = svrlst->next );

		svrlst->next = malloc(sizeof(*server));
		if ( NULL == svrlst->next )
		{
			return 0;
		}

		memcpy(svrlst->next, server, sizeof(*server));
	}

	return 1;
}

/*
 *	Connect to DDNS server.
 *
 *	@param[in]	context		: pointer to a DDNS context.
 *
 *	@return	If successful, it will return the connected socket. Otherwise,
 *			DDNS_INVALID_SOCKET will be returned.
 */
ddns_socket ddns_connect(struct ddns_context* context)
{
	int continue_trying = 1;
	const struct ddns_server* server = NULL;

	if ( NULL == context || DDNS_INVALID_SOCKET != context->socket )
	{
		return DDNS_INVALID_SOCKET;
	}

	/* try each DDNS server in the server list. */
	for (	server = context->server;
			(NULL != server) && (0 != continue_trying);
			server = server->next )
	{
		char **in_addr = NULL;
		struct hostent *addr = NULL;
		struct sockaddr_in svr_ip;

		/* check exit signal */
		if ( 0 != context->exit_signal )
		{
			/* stop trying */
			continue_trying = 0;
			break;
		}

		ddns_printf_n(	context,
						msg_type_info,
						"Connecting to '%s:%d'... ",
						server->domain,
						server->port );

		/* resolve server domain name to ip addresses. */
		ddns_printf_v(context, msg_type_info, "Resolving '%s'... ", server->domain);
		addr = (struct hostent*)gethostbyname(server->domain);
		if ( (0 == addr) || (0 == addr->h_addr_list) )
		{
			/* failed to resolve domain name, try next server. */
			ddns_printf_v(context, msg_type_info, "failed.\n");

			/* check exit signal */
			if ( 0 != context->exit_signal )
			{
				/* stop trying */
				continue_trying = 0;
			}
		}
		else
		{
			/* domain name resolve succeeded, proceed. */
			ddns_printf_v(context, msg_type_info, "done.\n");

			/* try each ip address of the DDNS server. */
			for ( in_addr = addr->h_addr_list; *in_addr; ++in_addr )
			{
				/* check exit signal */
				if ( 0 != context->exit_signal )
				{
					/* stop trying */
					continue_trying = 0;
					break;
				}

				/* construct server address */
				svr_ip.sin_family	= AF_INET;
				svr_ip.sin_port = htons(server->port);
				memcpy(&(svr_ip.sin_addr), *in_addr, (int)addr->h_length);

				/* preserve server address */
				context->active_server.sin_port = svr_ip.sin_port;
				context->active_server.sin_family = svr_ip.sin_family;
				memcpy(	&(context->active_server.sin_addr),
						*in_addr,
						(int)addr->h_length );

				/* create socket */
				ddns_printf_v(	context,
								msg_type_info,
								"Connecting to server[%d.%d.%d.%d:%d]... ",
								(int)(((unsigned char*)(*in_addr))[0]),
								(int)(((unsigned char*)(*in_addr))[1]),
								(int)(((unsigned char*)(*in_addr))[2]),
								(int)(((unsigned char*)(*in_addr))[3]),
								(int)server->port );
				context->socket = ddns_socket_create(AF_INET, SOCK_STREAM, 0);
				if ( DDNS_INVALID_SOCKET == context->socket )
				{
					/* create socket failed, stop further process. */
					continue_trying = 0;
					ddns_printf_v(context, msg_type_info, "failed.\n");
					break;
				}
				else
				{
					/* successfully created a socket */
					int result = -1;

					/* connect to server */
					result = ddns_socket_connect(	context->socket,
													(struct sockaddr*)&svr_ip,
													sizeof(svr_ip),
													context->timeout,
													&(context->exit_signal) );
					if ( 0 == result )
					{
						/* connected */
						ddns_printf_v(context, msg_type_info, "done.\n");
						ddns_printf_n(context, msg_type_info, "done.\n");
						goto _DONE;
					}
					else
					{
						/* failed, try next ip */
						ddns_printf_v(context, msg_type_info, "failed.\n");

						ddns_socket_close(context->socket);
						context->socket = DDNS_INVALID_SOCKET;
					}
				}
			}
		}

		/* failed to connect to a DDNS server. */
		ddns_printf_n(context, msg_type_info, "failed.\n");
	}

	/* failed to connect to any of the DDNS servers in server list. */
	ddns_printf_q(context, msg_type_error, "Failed to connect to server.\n");

_DONE:
	return context->socket;
}


/**
 *	Output a formatted text message, to either stdout of log file.
 *
 *	@param[in]	context		: pointer to a DDNS context.
 *	@param[in]	type		: type of the text message.
 *	@param[in]	format		: message format string, compatible with 'printf'.
 *	@param]in]	args		: arguments to format text message.
 */
void ddns_msg( struct ddns_context * context, enum ddns_msg_type type, const char * format, ...)
{
	va_list args;
	va_start(args, format);

	ddns_vmsg(context, type, format, args);

	va_end(args);
}

void ddns_vmsg(struct ddns_context * context, enum ddns_msg_type type, const char * format, va_list args)
{
	char msg[1024];
	int msglen = 0;
	char * prefix = NULL;
	FILE * out = NULL;
	FILE * out2 = NULL;

	switch(type)
	{
	case msg_type_debug:
		prefix = "DEBUG: ";
		out = stdout;
		out2 = context->stream_out;
		break;
	case msg_type_info:
		prefix = "";
		out = stdout;
		out2 = context->stream_out;
		break;
	case msg_type_warning:
		prefix = "WARNING: ";
		out = stdout;
		out2 = context->stream_out;
		break;
	case msg_type_error:
		prefix = "ERROR: ";
		out = stderr;
		out2 = context->stream_out;
		break;
	default:
		prefix = NULL;
		out = stdout;
		out2 = NULL;
		break;
	}

	msglen = c99_vsnprintf(msg, sizeof(msg), format, args);
	if (msglen > sizeof(msg))
	{
		msglen = sizeof(msg) - 1;
	}

	if (0 == context->log_status)
	{
		time_t t = 0;
		struct tm * tmp = NULL;
		char strtm[32];

		t = time(NULL);
		tmp = localtime(&t);
		strftime(strtm, sizeof(strtm), "%Y-%m-%d %H:%M:%S", tmp);

		fprintf(out, "[%s] %s%s", strtm, prefix, msg);
		if (NULL != out2)
		{
			fprintf(out2, "[%s] %s%s", strtm, prefix, msg);
		}
	}
	else
	{
		fprintf(out, "%s", msg);
		if (NULL != out2)
		{
			fprintf(out2, "%s", msg);
		}
	}
	fflush(out);
	if (NULL != out2)
	{
		fflush(out2);
	}

	if (msglen > 0 && '\n' == msg[msglen - 1])
	{
		context->log_status = 0;
	}
	else
	{
		context->log_status = 1;
	}
}


/**
 *	ddns_printf_q
 *
 *	Print quiet mode message, messages of other modes will be suppressed.
 */
void ddns_printf_q(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...)
{
	if ( verbose_quiet == context->verbose_mode )
	{
		va_list args;
		va_start(args, format);

		ddns_vmsg(context, type, format, args);

		va_end(args);
	}
}

/**
 *	ddns_printf_n
 *
 *	Print normal mode message, messages of other modes will be suppressed.
 */
void ddns_printf_n(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...)
{
	if ( verbose_normal == context->verbose_mode )
	{
		va_list args;
		va_start(args, format);

		ddns_vmsg(context, type, format, args);

		va_end(args);
	}
}

/**
 *	ddns_printf_v
 *
 *	Print verbose mode message, messages of other modes will be suppressed.
 */
void ddns_printf_v(struct ddns_context *context, enum ddns_msg_type type, const char *format, ...)
{
	if ( verbose_verbose == context->verbose_mode )
	{
		va_list args;
		va_start(args, format);

		ddns_vmsg(context, type, format, args);

		va_end(args);
	}
}


/**
 *	Wait until next update.
 *
 *	@param[in]	context	: the DDNS context.
 *
 *	@return		Return non-zero if the client should keep the domains on-line,
 *				otherwise return 0.
 */
int ddns_wait(struct ddns_context * context)
{
	int interval	= 0;
	int end_prog	= 1;

	while ( interval < context->interval )
	{
		ddns_sync_lock(&(context->sync_object));
		if ( context->exit_signal )
		{
			end_prog = 0;
		}
		ddns_sync_unlock(&(context->sync_object));

		if ( 0 == end_prog )
		{
			break;
		}

#if defined(WIN32)
		Sleep(1000);
#else
		sleep(1);
#endif

		++interval;
	}

	return end_prog;
}


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
int ddns_strcasecmp(const char* str1, const char* str2)
{
#if defined(WIN32)
	return _stricmp(str1, str2);
#else
	return strcasecmp(str1, str2);
#endif
}


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
int ddns_strncasecmp(const char* str1, const char* str2, size_t count)
{
#if defined(WIN32)
	return _strnicmp(str1, str2, count);
#else
	return strncasecmp(str1, str2, count);
#endif
}


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
const char * ddns_strcasestr(const char* str1, const char* str2)
{
	int				length		= 0;
	const char	*	position	= NULL;

	if ( (NULL == str1) || (NULL == str2) || ('\0' == (*str2)) )
	{
		return str1;
	}

	for ( length = strlen(str2); '\0' != (*str1); ++str1 )
	{
		if ( 0 == ddns_strncasecmp(str1, str2, length) )
		{
			position = str1;
			break;
		}
	}

	return position;
}


/**
 *	Convert error code to human readable string.
 *
 *	@param[in]	error_cide	: the error code.
 *
 *	@return		Pointer to the statically allocated human readable string.
 */
const char * ddns_err2str(ddns_error error_code)
{
	const char * message = "unknown error";

	switch (error_code & 0x7FFFFFFF)
	{
	case DDNS_ERROR_SUCCESS:
		message = "successful";
		break;
	case DDNS_ERROR_BADAUTH:
		message = "authentication failed, please check your account name and password";
		break;
	case DDNS_ERROR_TIMEOUT:
		message = "connection timeout";
		break;
	case DDNS_ERROR_BLOCKED:
		message = "your DDNS account is blocked";
		break;
	case DDNS_ERROR_BADSVR:
		message = "invalid DDNS server";
		break;
	case DDNS_ERROR_NOCHG:
		message = "IP address is not changed";
		break;
	case DDNS_ERROR_NOTIMPL:
		message = "required functionality is not implemented yet";
		break;
	case DDNS_ERROR_BADARG:
		message = "incorrect parameter";
		break;
	case DDNS_ERROR_INSUFFICIENT_MEMORY:
		message = "insufficient memory";
		break;
	case DDNS_ERROR_INSUFFICIENT_BUFFER:
		message = "insufficient buffer";
		break;
	case DDNS_ERROR_BADURL:
		message = "invalid URL";
		break;
	case DDNS_ERROR_CONNECTION:
		message = "connection error";
		break;
	case DDNS_ERROR_UNINIT:
		message = "uninitialized DDNS context";
		break;
	case DDNS_ERROR_BADAGENT:
		message = "bad user-agent";
		break;
	case DDNS_ERROR_PAIDFEATURE:
		message = "can't use paid feature";
		break;
	case DDNS_ERROR_NOHOST:
		message = "the host does not exist in domain";
		break;
	case DDNS_ERROR_NODOMAIN:
		message = "the domain does not exist in your account";
		break;
	case DDNS_ERROR_BADDOMAIN:
		message = "the domain name is not fully-qualified";
		break;
	case DDNS_ERROR_SVRDOWN:
		message = "the DDNS server is down";
		break;
	case DDNS_ERROR_DUPDOMAIN:
		message = "a domain with the same name already exists";
		break;
	case DDNS_ERROR_NXDOMAIN:
		message = "the domain name does not exist";
		break;
	case DDNS_ERROR_EMPTY:
		message = "there's no DNS record in this domain";
		break;
	case DDNS_ERROR_CONNRST:
		message = "connection reset by remote host";
		break;
	case DDNS_ERROR_CONNCLOSE:
		message = "connection closed by remote host";
		break;
	case DDNS_ERROR_UNREACHABLE:
		message = "server unreachable, typically because of network problem";
		break;
	case DDNS_ERROR_REDIRECT:
		message = "server redirection is requested";
		break;
	case DDNS_ERROR_MAXREDIRECT:
		message = "maximum server redirection count has been reached";
		break;
	case DDNS_ERROR_NO_SERVER:
		message = "No available server found";
		break;
	case DDNS_ERROR_SSL_REQUIRED:
		message = "SSL is not supported!";
		break;
	case DDNS_ERROR_INVALID_PROTO:
		message = "unsupported DDNS protocol.";
		break;
	case DDNS_ERROR_UNKNOWN:
	default:
		message = "unknown error";
		break;
	}

	return message;
}
