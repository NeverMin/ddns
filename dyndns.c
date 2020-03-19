/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: dyndns.c 289 2013-03-07 04:23:41Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2009-12-19.
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
 *  Interface for accessing DynDNS service.
 */

#include "dyndns.h"
#include <stdlib.h>			/* malloc            */
#include <stdio.h>			/* sscanf            */
#include <string.h>			/* memset            */
#include <assert.h>			/* assert            */
#include "ddns_string.h"	/* c99_snprintf, ... */
#include "http.h"			/* http_connect, ... */
#include "base64.h"			/* base64_encode     */

/*============================================================================*
 *	Local Macros & Constants
 *============================================================================*/

#define DYNDNS_MAX_HOASTNAME	20l

static const char	DYNDNS_URL_GETIP[]			= "http://checkip.dyndns.com/";

static const char	DYNDNS_CMD_UPDATE[]			= "/nic/update";
static const char	DYNDNS_CMD_HOSTLIST[]		= "/text/gethostlist";

/* successful: successful update */
static const char	DYNDNS_RETCODE_GOOD[]		= "good";
/* successful: IP address or other settings have not changed */
static const char	DYNDNS_RETCODE_NOCHG[]		= "nochg";
/* failure: host has been blocked */
static const char	DYNDNS_RETCODE_ABUSE[]		= "abuse";
/* failure: user agent is blocked */
static const char	DYNDNS_RETCODE_BADAGENT[]	= "badagent";
/* failure: user/pass pair bad */
static const char	DYNDNS_RETCODE_BADAUTH[]	= "badauth";
/* failure: bad system parameter */
static const char	DYNDNS_RETCODE_BADSYS[]		= "badsys";
/* failure: DNS inconsistency */
static const char DYNDNS_RETCODE_DNSERROR[]		= "dnserr";
/* failure: paid account feature */
static const char	DYNDNS_RETCODE_NOTDONATOR[]	= "!donator";
/* failure: no such host in system */
static const char	DYNDNS_RETCODE_NOHOST[]		= "nohost";
/* failure: invalid hostname format */
static const char	DYNDNS_RETCODE_BADDOMAIN[]	= "notfqdn";
/* failure: serious error */
static const char	DYNDNS_RETCODE_NUMHOST[]	= "numhost";
/* failure: host not in this account */
static const char	DYNDNS_RETCODE_NOTYOURS[]	= "!yours";
/* failure: there is a problem or scheduled maintenance on server. */
static const char	DYNDNS_RETCODE_SERVERDOWN[]	= "911";


/*============================================================================*
 *	Declaration of Local Types
 *============================================================================*/

/**
 *	Structure to keep dyndns specific information in DDNS context.
 */
struct dyndns_context
{
	char							ip_address[16];
	struct ddns_server			*	host_list;
};

/**
 *	Buffer used in function [dyndns_http_callback].
 */
struct dyndns_buffer
{
	size_t			size;
	size_t			used;
	char		*	buffer;
};


/**
 *	Buffer used in function [dyndns_parse_hostname].
 */
struct dyndns_hostname_buffer
{

	enum dyndns_hostname_status
	{
		dyndns_account_type,
		dyndns_host_name
	}								status;
	char							account_type[32];
	struct ddns_server			*	host_list;
};

/**
 *	Mapping record of DynDNS return code to DDNS error code.
 */
struct dyndns_error_code_map
{
	ddns_error						error_code;
	const char					*	ret_code;
};


/*============================================================================*
 *	Declaration of Local Functions
 *============================================================================*/

/**
 *	Initialize DDNS context for DynDNS service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error dyndns_interface_initialize(struct ddns_context * context);


/**
 *	Dummy function for [ddns_interface] object.
 *
 *	@return		Always return DDNS_ERROR_SUCCESS.
 */
static ddns_error dyndns_interface_get_ip_address(
	struct ddns_context	*	context,
	char				*	buffer,
	size_t					length
	);


/**
 *	Determine if IP address is changed since last call.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if IP address is changed since last
 *				call, or return DDNS_ERROR_NOCHG if IP address isn't changed.
 *				If any error occurred during the call, an error code will be
 *				returned.
 */
static ddns_error dyndns_interface_is_ip_changed(struct ddns_context * context);


/**
 *	Determine if IP address is changed since last call.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_interface_do_update(struct ddns_context * context);


/**
 *	Finalize DDNS context for DynDNS service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_interface_finalize(struct ddns_context * context);


/**
 *	Destroy DDNS interface object created by [dyndns_create_interface].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error dyndns_interface_destroy(ddns_interface* interface);


/**
 *	Get internet IP address of local computer.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out]	text_buffer	: buffer to save the IP address.
 *	@param[in[	buffer_size	: size of the buffer.
 *
 *	@note		The returned IP address is like "123.123.123.123".
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] if successfully get the internet
 *				IP address, otherwise return an error code.
 */
static ddns_error dyndns_get_ip_address(
	struct ddns_context	*	context,
	char				*	text_buffer,
	int						buffer_size
	);


/**
 *	HTTP callback, save all received bytes to a buffer.
 *
 *	@param[in]	chr		: newly received data.
 *	@param[in]	buffer	: buffer to save the received data.
 */
static void dyndns_http_callback(char chr, struct dyndns_buffer* buffer);


/**
 *	Callback function to parse host name list.
 *
 *	@param[in]	chr		: newly received data.
 *	@param[out]	list	: the host name list.
 */
static void dyndns_parse_hostname(char chr, struct dyndns_hostname_buffer *list);


/**
 *	Send command to DynDNS server.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	command		: command to be sent to server.
 *	@param[in]	callback	: callback function for handling server response. If
 *							  it's NULL, [dyndns_http_callback] will be used.
 *	@param[out]	buffer		: to keep the response from server.
 *
 *	@return		If the command is
 */
static ddns_error dyndns_send_command(
	struct ddns_context		*	context,
	const char				*	command,
	http_callback				callback,
	void					*	buffer
	);


/**
 *	Get a list of host names under your DynDNS account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out]	error_code	:
 *
 *	@return		List of host names. If no host name found, NULL will be returned.
 */
static ddns_error dyndns_get_hostlist(
	struct ddns_context	*	context,
	struct ddns_server	**	host_list
	);


/**
 *	Append host names to a url, separated by comma.
 *
 *	@param[in/out]	host_list	: list of host names
 *	@param[in]		max_cnt		: maximum count of host names appended to url.
 *	@param[out]		buffer		: the url to append host names to.
 *	@param[in]		buffer_size	: size of [buffer] in characters.
 *
 *	@note		It will add 20 host names at maximum for each call. After
 *				returned, [host_list] will point to the list of the remaining
 *				host names.
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_url_append_hostnames(
	struct ddns_server	**	host_list,
	int						max_cnt,
	char				*	buffer,
	size_t					buffer_size
	);


/**
 *	Convert return code from DynDNS server to local error code.
 *
 *	@param[in]		ret_code	: the return code to be checked.
 *
 *	@return		Associated error code with the return code.
 */
static ddns_error dyndns_check_return_code(
	const char			*	ret_code
	);


/**
 *	Determine if an error is critical.
 *
 *	@param[in]		error_code	: the error code to be tested.
 *
 *	@note		For critical errors, only one error code is returned from DDNS
 *				server for a request with multiple host names.
 *
 *	@return		Return non-zero if it's a critical error, otherwise return 0.
 */
static int dyndns_is_critical_err(ddns_error error_code);


/*============================================================================*
 *	Implementation of Functions
 *============================================================================*/

/**
 *	Create DDNS interface for accessing DynDNS service.
 *
 *	@return		Return pointer to the newly created interface object, or NULL
 *				if there's an error creating interface object.
 */
ddns_interface * dyndns_create_interface(void)
{
	ddns_interface * ddns = NULL;

	ddns = (ddns_interface*)malloc(sizeof(ddns_interface));
	if ( NULL != ddns )
	{
		ddns->initialize		= &dyndns_interface_initialize;
		ddns->is_ip_changed		= &dyndns_interface_is_ip_changed;
		ddns->get_ip_address	= &dyndns_interface_get_ip_address;
		ddns->do_update			= &dyndns_interface_do_update;
		ddns->finalize			= &dyndns_interface_finalize;
		ddns->destroy			= &dyndns_interface_destroy;
	}

	return ddns;
}


/**
 *	Initialize DDNS context for DynDNS service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error dyndns_interface_initialize(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_context	*	dyndns		= NULL;

	if ( (NULL == context) || (proto_dyndns != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

#if !defined(HTTP_SUPPORT_SSL) || 0 == HTTP_SUPPORT_SSL
	ddns_msg(context, msg_type_warning, "SSL isn't supported! Your password will sent in plain-text.\n");
#endif

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 != ddns_create_extra_param(context, sizeof(*dyndns)) )
		{
			dyndns = (struct dyndns_context*)context->extra_data;
		}
		else
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}
	}

	/**
	 *	Unacceptable Client Behavior:
	 *
	 *	Access our web-based IP detection script (http://checkip.dyndns.com/)
	 *	more than once every 10 minutes.
	 *
	 *	Please check here for more information:
	 *
	 *		http://www.dyndns.com/developers/specs/policies.html
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( context->interval < 10 * 60 )
		{
			context->interval = 10 * 60;
			ddns_msg(context,	msg_type_warning,
								"Update interval changed to %ds.\n",
								context->interval
								);
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_printf_v(context, msg_type_info, "Get internet IP address... ");
		error_code = dyndns_get_ip_address(	context,
											dyndns->ip_address,
											_countof(dyndns->ip_address)
											);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			ddns_printf_v(context, msg_type_info, "%s\n", dyndns->ip_address);
		}
		else
		{
			ddns_printf_v(context, msg_type_info, "failed.\n");
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( NULL == context->domain )
		{
			ddns_printf_v(context, msg_type_info, "Get host name list... ");
			dyndns->host_list = context->domain;
			error_code = dyndns_get_hostlist(context, &(context->domain));
			if (DDNS_ERROR_SUCCESS == error_code && NULL != context->domain)
			{
				struct ddns_server * host = context->domain;
				for (; NULL != host; host = host->next)
				{
					ddns_printf_v(context, msg_type_info, "\n  * %s", host->domain);
				}
				ddns_printf_v(context, msg_type_info, "\n");
			}
			else
			{
				ddns_printf_v(context, msg_type_info, "failed.\n");
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dyndns_interface_do_update(context);
	}

	return error_code;
}


/**
 *	Determine if IP address is changed since last call.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if IP address is changed since last
 *				call, or return DDNS_ERROR_NOCHG if IP address isn't changed.
 *				If any error occurred during the call, an error code will be
 *				returned.
 */
static ddns_error dyndns_interface_is_ip_changed(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_context	*	dyndns		= NULL;
	char						ip_address[_countof(dyndns->ip_address)];

	if ( (NULL == context) || (proto_dyndns != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dyndns = (struct dyndns_context*)context->extra_data;
		if ( NULL == dyndns )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dyndns_get_ip_address(	context,
											ip_address,
											_countof(ip_address)
											);
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 == strcmp(ip_address, dyndns->ip_address) )
		{
			error_code = DDNS_ERROR_NOCHG;
		}
		else
		{
			c99_strncpy(dyndns->ip_address,
						ip_address,
						_countof(dyndns->ip_address)
						);
		}
	}

	return error_code;
}


/**
 *	Dummy function for [ddns_interface] object.
 *
 *	@return		Always return DDNS_ERROR_NOTIMPL.
 */
static ddns_error dyndns_interface_get_ip_address(
	struct ddns_context	*	context,
	char				*	buffer,
	size_t					length
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_context	*	dyndns		= NULL;

	if ( (NULL == context) || (proto_dyndns != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dyndns = (struct dyndns_context*)context->extra_data;
		if ( NULL == dyndns )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		c99_strncpy(buffer, dyndns->ip_address, length);
	}

	return error_code;
}


/**
 *	Determine if IP address is changed since last call.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_interface_do_update(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_context	*	dyndns		= NULL;

	if ( (NULL == context) || (proto_dyndns != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dyndns = (struct dyndns_context*)context->extra_data;
		if ( NULL == dyndns )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t					size	= 0;
		char				*	command	= NULL;
		struct ddns_server	*	domain	= NULL;

		domain = context->domain;
		while ( (NULL != domain) && (DDNS_ERROR_SUCCESS == error_code) )
		{
			size_t						len			= 0;
			const size_t				step		= 1024;
			struct ddns_server		*	domain_list	= domain;
			struct dyndns_buffer		buffer;
			char						response[1024] = { 0 };

			len = c99_snprintf(command,	size,
										"%s?hostname=",
										DYNDNS_CMD_UPDATE
										);
			if ( len >= size )
			{
				/* insufficient buffer: retry with a larger buffer */
				char * new_command = realloc(command, size + step);
				if ( NULL != new_command )
				{
					size	+= step;
					command	= new_command;
					continue;
				}
				else
				{
					error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
					break;
				}
			}

			error_code = dyndns_url_append_hostnames(	&domain,
														DYNDNS_MAX_HOASTNAME,
														command + len,
														size - len
														);
			if ( DDNS_ERROR_INSUFFICIENT_BUFFER == error_code )
			{
				/* insufficient buffer: retry with a larger buffer */
				char * new_command = realloc(command, size + step);
				if ( NULL != new_command )
				{
					size		+= step;
					command		= new_command;
					error_code	= DDNS_ERROR_SUCCESS;
					continue;
				}
				else
				{
					error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
					break;
				}
			}
			else if ( DDNS_ERROR_SUCCESS != error_code )
			{
				break;
			}

			ddns_printf_v(context, msg_type_info, "Updating IP address(es)... ");
			buffer.size		= sizeof(response);
			buffer.used		= 0;
			buffer.buffer	= &(response[0]);
			error_code = dyndns_send_command(	context,
												command,
												NULL,
												&buffer
												);
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				size_t idx = 0;

				if ( buffer.used >= buffer.size )
				{
					error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
					break;
				}

				/* replace line-feed/carriage return to '\0' */
				for ( idx = 0; idx < buffer.used; ++idx )
				{
					switch ( buffer.buffer[idx] )
					{
					case '\r':
					case '\n':
						buffer.buffer[idx] = '\0';
						break;

					default:
						break;
					}
				}

				for ( ; NULL != domain_list; domain_list = domain_list->next )
				{
					ddns_error sub_err = DDNS_ERROR_SUCCESS;
					sub_err = dyndns_check_return_code(buffer.buffer);
					ddns_printf_v(context,	msg_type_info,
											"\n  * %-32s : %s.",
											domain_list->domain,
											buffer.buffer
											);
					if ( (DDNS_ERROR_NOCHG != sub_err) && (DDNS_ERROR_SUCCESS != sub_err) )
					{
						error_code = sub_err;
					}

					if ( 0 == dyndns_is_critical_err(sub_err) )
					{
						buffer.buffer += strlen(buffer.buffer) + 1;
					}
				}
				ddns_printf_v(context, msg_type_info, "\n");

				if ( DDNS_ERROR_SUCCESS != error_code )
				{
					break;
				}
			}
			else
			{
				ddns_printf_v(context, msg_type_info, "failed.\n");
			}
		}

		if ( NULL != command )
		{
			free(command);
			command = NULL;
		}
	}

	return error_code;
}


/**
 *	Finalize DDNS context for DynDNS service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_interface_finalize(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_context	*	dyndns		= NULL;

	if ( (NULL == context) || (proto_dyndns != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dyndns = (struct dyndns_context*)context->extra_data;
		if ( NULL == dyndns )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( NULL == dyndns->host_list )
		{
			struct ddns_server * svrlst = context->domain;
			for ( ; NULL != svrlst; svrlst = context->domain)
			{
				context->domain = svrlst->next;
				free(svrlst);
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_remove_extra_param(context);
	}

	return error_code;
}


/**
 *	Destroy DDNS interface object created by [dyndns_create_interface].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error dyndns_interface_destroy(ddns_interface* interface)
{
	ddns_error error_code = DDNS_ERROR_SUCCESS;

	if ( NULL != interface )
	{
		if ( interface->destroy == &dyndns_interface_destroy )
		{
			free(interface);
			interface = NULL;
		}
		else if ( NULL != interface->destroy )
		{
			error_code = interface->destroy(interface);
		}
		else
		{
			error_code = DDNS_ERROR_NOTIMPL;
		}
	}

	return error_code;
}


/**
 *	Get internet IP address of local computer.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out]	text_buffer	: buffer to save the IP address.
 *	@param[in[	buffer_size	: size of the buffer.
 *
 *	@note		The returned IP address is like "123.123.123.123".
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] if successfully get the internet
 *				IP address, otherwise return an error code.
 */
static ddns_error dyndns_get_ip_address(
	struct ddns_context	*	context,
	char				*	text_buffer,
	int						buffer_size
	)
{
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;
	struct http_request	*	request		= NULL;

	if ( (NULL == context) || (buffer_size <= 0) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		request = http_create_request(	http_method_get,
										DYNDNS_URL_GETIP,
										context->timeout
										);
		if ( NULL == request )
		{
			error_code = DDNS_ERROR_BADURL;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		switch ( http_connect(request) )
		{
		case 0:
			/* successful, do nothing */
			break;

		case ETIMEDOUT:
			error_code = DDNS_ERROR_TIMEOUT;
			break;

		case ENETUNREACH:
			error_code = DDNS_ERROR_UNREACHABLE;
			break;

		default:
			error_code = DDNS_ERROR_CONNECTION;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int http_status = http_send_request(request, "", 0);
		if ( http_status < 200 || http_status >= 300 )
		{
			switch( ddns_socket_get_errno() )
			{
			case ETIMEDOUT:
				error_code = DDNS_ERROR_TIMEOUT;
				break;

			case ENETUNREACH:
				error_code = DDNS_ERROR_UNREACHABLE;
				break;

			default:
				error_code = DDNS_ERROR_CONNECTION;
				break;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		struct dyndns_buffer	buffer;
		int						result		= 0;
		char					html[1024]	= { '\0' };

		buffer.size		= _countof(html);
		buffer.buffer	= html;
		buffer.used		= 0;
		result = http_get_response(	request,
									(http_callback)&dyndns_http_callback,
									&buffer
									);
		if ( (0 == result) || (buffer.used <= 0) )
		{
			error_code = DDNS_ERROR_CONNECTION;
		}
		else
		{
			const char		sig_start[] = "Current IP Address: ";
			const char		sig_end[]	= "</body>";
			const char	*	pos_start	= NULL;
			const char	*	pos_end		= NULL;

			html[buffer.used - 1] = '\0';
			pos_start	= ddns_strcasestr(html, sig_start);
			pos_end		= ddns_strcasestr(html, sig_end);
			if ( (NULL == pos_start) || (NULL == pos_end) )
			{
				error_code = DDNS_ERROR_CONNECTION;
			}
			else
			{
				pos_start += _countof(sig_start) - 1;
				if ( pos_end <= pos_start )
				{
					error_code = DDNS_ERROR_CONNECTION;
				}
			}
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				int length = c99_snprintf(	text_buffer,
											buffer_size,
											"%.*s",
											pos_end - pos_start,
											pos_start
											);
				if ( length >= buffer_size )
				{
					error_code = DDNS_ERROR_CONNECTION;
				}
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int		d1		= 0;
		int		d2		= 0;
		int		d3		= 0;
		int		d4		= 0;

		if ( 4 != sscanf(text_buffer, "%d.%d.%d.%d", &d1, &d2, &d3, &d4) )
		{
			error_code = DDNS_ERROR_CONNECTION;
		}
		else
		{
			/* If it's a valid IP address? */
			if (	(d1 < 0) || (d1 >= 256) || (d2 < 0) || (d2 >= 256)
				||	(d3 < 0) || (d3 >= 256) || (d4 < 0) || (d4 >= 256)
				||	((0 == d1) && (0 == d2) && (0 == d3) && (0 == d4))
				||	((255 == d1) && (255 == d2) && (255 == d3) && (255 == d4)) )
			{
				error_code = DDNS_ERROR_CONNECTION;
			}

			/* If it's a intranet address? */
			if (	(10 == d1)
				||	((172 == d1) && (16 <= d2) && (31>= d2))
				||	((192 == d1) && (168 == d2) && (0 == d3))
				||	((169 == d1) && (254 == d2)) )
			{
				error_code = DDNS_ERROR_CONNECTION;
			}
		}
	}

	http_destroy_request(request);
	request = NULL;

	return error_code;
}


/**
 *	HTTP callback, save all received bytes to a buffer.
 *
 *	@param[in]	chr		: newly received data.
 *	@param[in]	buffer	: buffer to save the received data.
 */
static void dyndns_http_callback(char chr, struct dyndns_buffer* buffer)
{
	if ( (NULL != buffer) && (NULL != buffer->buffer) )
	{
		if ( buffer->used < buffer->size )
		{
			buffer->buffer[buffer->used] = chr;
			++(buffer->used);
		}
		else
		{
			/* buffer overflow */
			assert(0);
		}
	}
}


/**
 *	Send command to DynDNS server.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	command		: command to be sent to server.
 *	@param[in]	callback	: callback function for handling server response. If
 *							  it's NULL, [dyndns_http_callback] will be used.
 *	@param[out]	buffer		: to keep the response from server.
 *
 *	@return		If the command is
 */
static ddns_error dyndns_send_command(
	struct ddns_context		*	context,
	const char				*	command,
	http_callback				callback,
	void					*	buffer
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct http_request		*	request		= NULL;

	if ( (NULL == context) || (NULL == command) || (NULL == context->server) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( NULL == callback )
	{
		callback = (http_callback)&dyndns_http_callback;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t	length		= 0;
		char	url[1024]	= { 0 };
		length = c99_snprintf(url,	_countof(url),
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
									"https://%s:%d%s",
#else
									"http://%s:%d%s",
#endif
									context->server->domain,
									(int)context->server->port,
									command
									);
		if ( length >= _countof(url) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			request = http_create_request(	http_method_get,
											url,
											context->timeout
											);
			if ( NULL == request )
			{
				error_code = DDNS_ERROR_BADURL;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		switch ( http_connect(request) )
		{
		case 0:
			/* successful, do nothing */
			break;

		case ETIMEDOUT:
			error_code = DDNS_ERROR_TIMEOUT;
			break;

		case ENETUNREACH:
			error_code = DDNS_ERROR_UNREACHABLE;
			break;

		default:
			error_code = DDNS_ERROR_CONNECTION;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t					length				= 0;
		char					auth[1024]			= { 0 };
		char					auth_base64[1024]	= { 0 };

		length = c99_snprintf(auth,	_countof(auth),
							  "%s:%s",
							  context->username,
							  context->password
							  );
		if ( length >= _countof(auth) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			length = base64_encode(	(unsigned char*)auth,
								   length,
								   auth_base64,
								   _countof(auth_base64)
								   );
			if ( length >= _countof(auth_base64) )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			length = c99_snprintf(auth, _countof(auth), "Basic %s", auth_base64);
			if ( length >= _countof(auth) )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			http_add_header(request, "Authorization", auth, 1);
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		/**
		 *	The HTTP status will not indicate any particular message. Rely on
		 *	the return codes instead.
		 */
		int http_status = http_send_request(request, NULL, 0);
		if ( 0 == http_status )
		{
			switch( ddns_socket_get_errno() )
			{
			case ETIMEDOUT:
				error_code = DDNS_ERROR_TIMEOUT;
				break;

			case ENETUNREACH:
				error_code = DDNS_ERROR_UNREACHABLE;
				break;

			default:
				error_code = DDNS_ERROR_CONNECTION;
				break;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int result = http_get_response(	request,
										(http_callback)callback,
										buffer
										);
		if ( 0 == result )
		{
			if ( ETIMEDOUT == ddns_socket_get_errno() )
			{
				error_code = DDNS_ERROR_TIMEOUT;
			}
			else
			{
				error_code = DDNS_ERROR_CONNECTION;
			}
		}
	}

	http_destroy_request(request);

	return error_code;
}


/**
 *	Callback function to parse host name list.
 *
 *	@param[in]	chr		: newly received data.
 *	@param[out]	list	: the host name list.
 */
static void dyndns_parse_hostname(char chr, struct dyndns_hostname_buffer* list)
{
	if ( NULL != list )
	{
		switch ( list->status )
		{
		case dyndns_account_type:
			if ( ':' == chr )
			{
				list->status = dyndns_host_name;
			}
			else
			{
				char str[] = { chr, '\0' };
				c99_strncat(list->account_type,
							str,
							_countof(list->account_type)
							);
			}
			break;

		case dyndns_host_name:
			if ( '\n' == chr )
			{
				list->status = dyndns_account_type;
				memset(list->account_type, 0, sizeof(list->account_type));
			}
			else if ( 0 == strcmp(list->account_type, "dyndns") )
			{
				struct ddns_server * host = NULL;
				if ( (NULL == list->host_list) || ',' == chr )
				{
					host = (struct ddns_server*)malloc(sizeof(*host));
					if ( NULL != host )
					{
						memset(host, 0, sizeof(*host));
						host->next = list->host_list;
						list->host_list = host;
					}
					if ( ',' == chr )
					{
						break;
					}
				}
				else
				{
					host = list->host_list;
				}
				if ( NULL != host )
				{
					char str[2] = { chr, '\0' };
					c99_strncat(host->domain, str, _countof(host->domain));
				}
			}
			else
			{
				/* host names of unsupported types, ignore */
			}
			break;
		}
	}
}


/**
 *	Get a list of host names under your DynDNS account.
 *
 *	@param[in]	context	: the DDNS context.
 *
 *	@return		List of host names. If no host name found, NULL will be returned.
 */
static ddns_error dyndns_get_hostlist(
	struct ddns_context	*	context,
	struct ddns_server	**	host_list
	)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct dyndns_hostname_buffer	buffer;

	buffer.status		= dyndns_account_type;
	buffer.host_list	= NULL;
	memset(&buffer.account_type, 0, sizeof(buffer.account_type));

	error_code = dyndns_send_command(	context,
										DYNDNS_CMD_HOSTLIST,
										(http_callback)&dyndns_parse_hostname,
										&buffer
										);
	if ( (DDNS_ERROR_SUCCESS == error_code) && (NULL == buffer.host_list) )
	{
		/* check why we get no host name from server */
		if ( dyndns_account_type == buffer.status )
		{
			error_code = dyndns_check_return_code(buffer.account_type);
		}
	}

	if ( NULL != host_list )
	{
		(*host_list) = buffer.host_list;
	}

	return error_code;
}


/**
 *	Append host names to a url, separated by comma.
 *
 *	@param[in/out]	host_list	: list of host names
 *	@param[in]		max_cnt		: maximum count of host names appended to url.
 *	@param[out]		buffer		: the url to append host names to.
 *	@param[in]		buffer_size	: size of [buffer] in characters.
 *
 *	@note		It will add 20 host names at maximum for each call. In successful
 *				return, [host_list] will point to the list of the remaining
 *				host names, otherwise it keeps unchanged.
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dyndns_url_append_hostnames(
	struct ddns_server	**	host_list,
	int						max_cnt,
	char				*	buffer,
	size_t					buffer_size
	)
{
	int						domain_cnt		= 0;
	size_t					length			= 0;
	ddns_error				error_code		= DDNS_ERROR_SUCCESS;
	struct ddns_server	*	original_list	= NULL;

	if ( (NULL == buffer) || (0 == buffer_size) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	if ( (NULL == host_list) || (NULL == (*host_list)) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		original_list = (*host_list);
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		for (domain_cnt = 0; domain_cnt < max_cnt; ++domain_cnt)
		{
			size_t domain_len = 0;

			if ( 0 != domain_cnt )
			{
				domain_len = http_urlencode(",",
											buffer + length,
											buffer_size - length
											);
				if ( domain_len < buffer_size - length )
				{
					length += domain_len;
				}
				else
				{
					error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
					break;
				}
			}

			domain_len = http_urlencode((*host_list)->domain,
										buffer + length,
										buffer_size - length
										);
			if ( domain_len < buffer_size - length )
			{
				length += domain_len;
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
				break;
			}

			(*host_list) = (*host_list)->next;
			if ( NULL == (*host_list) )
			{
				break;
			}
		}
	}

	if ( DDNS_ERROR_INSUFFICIENT_BUFFER == error_code )
	{
		(*host_list) = original_list;
	}

	return error_code;
}


/**
 *	Convert return code from DynDNS server to local error code.
 *
 *	@param[in/out]	ret_code	: the return code to be checked.
 *
 *	@return		Associated error code with the return code.
 */
static ddns_error dyndns_check_return_code(
	const char			*	ret_code
	)
{
	int			idx			= 0;
	ddns_error	error_code	= DDNS_ERROR_UNKNOWN;

	static const struct dyndns_error_code_map error_code_map[] =
	{
		{ DDNS_ERROR_SUCCESS,		DYNDNS_RETCODE_GOOD			},
		{ DDNS_ERROR_NOCHG,			DYNDNS_RETCODE_NOCHG		},
		{ DDNS_ERROR_BLOCKED,		DYNDNS_RETCODE_ABUSE		},
		{ DDNS_ERROR_BADAGENT,		DYNDNS_RETCODE_BADAGENT		},
		{ DDNS_ERROR_BADARG,		DYNDNS_RETCODE_BADSYS		},
		{ DDNS_ERROR_UNKNOWN,		DYNDNS_RETCODE_DNSERROR		},
		{ DDNS_ERROR_PAIDFEATURE,	DYNDNS_RETCODE_NOTDONATOR	},
		{ DDNS_ERROR_NOHOST,		DYNDNS_RETCODE_NOHOST		},
		{ DDNS_ERROR_BADDOMAIN,		DYNDNS_RETCODE_BADDOMAIN	},
		{ DDNS_ERROR_UNKNOWN,		DYNDNS_RETCODE_NUMHOST		},
		{ DDNS_ERROR_NOHOST,		DYNDNS_RETCODE_NOTYOURS		},
		{ DDNS_ERROR_SVRDOWN,		DYNDNS_RETCODE_SERVERDOWN	},
		{ DDNS_FATAL_ERROR(DDNS_ERROR_BADAUTH),	DYNDNS_RETCODE_BADAUTH }
	};

	for ( idx = 0; idx < _countof(error_code_map); ++idx )
	{
		int length = strlen(error_code_map[idx].ret_code);
		int result = ddns_strncasecmp(	error_code_map[idx].ret_code,
										ret_code,
										length
										);
		if ( 0 == result )
		{
			error_code	= error_code_map[idx].error_code;
			break;
		}
	}

	return error_code;
}


/**
 *	Determine if an error is critical.
 *
 *	@param[in]		error_code	: the error code to be tested.
 *
 *	@note		For critical errors, only one error code is returned from DDNS
 *				server for a request with multiple host names.
 *
 *	@return		Return non-zero if it's a critical error, otherwise return 0.
 */
static int dyndns_is_critical_err(ddns_error error_code)
{
	int is_critical = 0;

	switch (error_code & 0x7FFFFFFF)
	{
	case DDNS_ERROR_BADAUTH:
	case DDNS_ERROR_BADAGENT:
	case DDNS_ERROR_SVRDOWN:
		is_critical = 1;
		break;

	default:
		is_critical = 0;
		break;
	}

	return is_critical;
}
