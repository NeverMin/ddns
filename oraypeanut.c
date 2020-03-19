/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: oraypeanut.c 289 2013-03-07 04:23:41Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2008-03-25.
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
 *  Interface for accessing oray peanuthull DDNS service.
 */

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "oraypeanut.h"
#include <string.h>			/* memset                */
#include <stdlib.h>			/* malloc, realloc       */
#include <stdarg.h>			/* va_list, ...          */
#include <assert.h>			/* assert                */
#include "hmac.h"			/* hmac_md5              */
#include "base64.h"			/* base64_encode, ...    */
#include "blowfish.h"		/* Blowfish_Encrypt, ... */
#include "ddns_socket.h"	/* ddns_socket, ...      */
#include "ddns_string.h"	/* c99_snprintf, ...     */

#ifdef TIME_WITH_SYS_TIME
#	include <sys/time.h>
#	include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#	include <sys/time.h>
#else
#	include <time.h>
#endif


/*============================================================================*
 *	Local Macros & Constants
 *============================================================================*/

/**
 *	Define it to "1" to compile it for hardware router (different protocol).
 *
 *	IMPORTANT:
 *		Before you can compile a hardware version, you will have to purchase
 *		a license from Oray. After you get a license from Oray, change the
 *		following values to yours:
 *
 *			- PEANUTHULL_CLIENT_ID	: client No. get from Oray.
 *			- PEANUTHULL_AUTH_KEY	: authentication key get from Oray.
 *			- PEANUTHULL_CLIENT_VER	: client program version
 */
#define PEANUTHULL_HARDWARE_ROUTER		1


/**
 *	These authentication values are provided from Oray.
 */
#define PEANUTHULL_CLIENT_ID		0x26A0
#define PEANUTHULL_CLIENT_VER		0x9899
#define PEANUTHULL_AUTH_KEY			0x1E0808B7


/**
 *	TCP session commands.
 */
#define PEANUTHULL_CMD_AUTH			"auth router6"
#define PEANUTHULL_CMD_REGISTER		"regi a %s"
#define PEANUTHULL_CMD_CONFIRM		"cnfm"
#define PEANUTHULL_CMD_USERINFO		"stat user"
#define PEANUTHULL_CMD_DOMAININFO	"stat domain"
#define PEANUTHULL_CMD_QUIT			"quit"

/**
 *	Maximum allowed server redirection count.
 */
#define PEANUTHULL_MAX_REDIRECTION	5

/**
 *	Macros for [peanuthull_send_request].
 */
#define PEANUTHULL_KEEPALIVE_REQ	0x00002010	/* keep-alive request    */
#define PEANUTHULL_KEEPALIVE_POS	0x00002050	/* keep-alive successful */
#define PEANUTHULL_KEEPALIVE_NEG	0x000003e8	/* keep-alive failed     */
#define PEANUTHULL_LOGOUT_REQ		0x0000000b	/* logout request        */
#define PEANUTHULL_LOGOUT_POS		0x00000033	/* logout successful     */
#define PEANUTHULL_LOGOUT_NEG		0x000003e9	/* logout failed         */


/*============================================================================*
 *	Declaration of Local Types & Functions
 *============================================================================*/

/**
 *	Keep-alive packet send to peanuthull server.
 */
struct peanuthull_keepalive_request
{
	ddns_ulong32	session_id;
	ddns_ulong32	data_type;
	ddns_ulong32	sequence;
	ddns_ulong32	checksum;
	ddns_ulong32	reserved;
};

/**
 *	Keep-alive response packet received from peanuthull server.
 */
struct peanuthull_keepalive_reponse
{
	ddns_ulong32	session_id;
	ddns_ulong32	data_type;
	ddns_ulong32	sequence;
	ddns_ulong32	checksum;
	ddns_ulong32	reserved1;
	ddns_ulong32	address;
	ddns_ulong32	reserved2;
	ddns_ulong32	reserved3;
};

/**
 *	Keep-alive context.
 */
struct peanuthull_keepalive_ctx
{
	BLOWFISH_CTX							encrypt_ctx;
	struct peanuthull_keepalive_request		request;
};

/**
 *	Structure to keep oray-peanuthull specific information in DDNS context.
 */
struct peanuthull_context
{
	ddns_socket							sock;
	char								buffer[1024];
	ddns_ulong32						address;
	struct ddns_server				*	redirect_to;
	struct ddns_server				*	domain_list;
	struct peanuthull_keepalive_ctx		keep_alive;
	int									failure_cnt;
	int									binary_mode;
};


/**
 *	Initialize DDNS context for oray-peanuthull service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error peanuthull_interface_initialize(
	struct ddns_context * context
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
static ddns_error peanuthull_interface_is_ip_changed(
	struct ddns_context * context
	);


/**
 *	Get current IP address.
 *
 *	@param[out]	buffer	: buffer to save the IP address.
 *	@param[in]	length	: size of the buffer in characters.
 */
static ddns_error peanuthull_interface_get_ip_address(
	struct ddns_context *	context,
	char				*	buffer,
	size_t					length
	);


/**
 *	Update all records via oray-peanuthull service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error peanuthull_interface_do_update(struct ddns_context * context);


/**
 *	Finalize DDNS context for oray-peanuthull service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error peanuthull_interface_finalize(struct ddns_context * context);


/**
 *	Destroy DDNS interface object created by [peanuthull_interface_create].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error peanuthull_interface_destroy(ddns_interface* ddns);


/**
 *	Calculate HMAC authentication digest for peanuthull.
 *
 *	@param[in]	text		: password
 *	@param[in]	text_len	: length of password in bytes
 *	@param[in]	key			: challenge
 *	@param[in]	key_len		: length of challenge in bytes
 *	@param[out]	digest		: buffer for generated digest
 *	@param[in]	digest_len	: size of output buffer in bytes
 *
 *	@return	length of the digest in bytes.
 */
static int peanuthull_hmac(
	unsigned char	*	text,
	size_t				text_len,
	unsigned char	*	key,
	size_t				key_len,
	unsigned char	*	digest,
	size_t				digest_len
	);


/**
 *	Initialize peanuthull blowfish context after we get key from server.
 *
 *	@param[in]	ctx		: Blowfish context to be initialized.
 *	@param[in]	key		: Private key to initialize blowfish context.
 *	@param[in]	keyLen	: Length of the private key in bytes.
 */
static void peanuthull_keepalive_init(
	BLOWFISH_CTX	*	ctx,
	unsigned char	*	key,
	size_t				keyLen
	);


/**
 *	Receive a message from DDNS server.
 *
 *	@param[in]	context			: DDNS context
 *	@param[out]	buffer			: buffer to store response from server
 *	@param[in]	buffer_len		: size of the buffer in bytes
 *	@param[out]	error_code		: the error code if -1 is returned.
 *
 *	@return If successful, it will return count of characters received from
 *			server excluding null terminator. Otherwise, -1 will be returned
 *			to indicate an error.
 */
static int peanuthull_recv(
   struct ddns_context	*	context,
   char					*	buffer,
   int						buffer_len,
   ddns_error			*	error_code
   );


/**
 *	peanuthull_sendcmd
 *
 *	Send a command to DDNS server.
 *
 *	@param[in]	sock		- socket to send command
 *	@param[in]	cmd			- the command to send to server
 *	@param[in]	sync_call	- whether to wait for response from server
 *
 *	@return	Return [DDNS_ERROR_SUCCESS] if the command is successfully sent to
 *			server. Otherwise an error code will be returned.
 *
 *	@note	If 'cmd' is PEANUTHULL_CMD_QUIT, the tcp connection will be closed
 *			before returning.
 */
static ddns_error peanuthull_sendcmd(
	struct ddns_context *	context,
	char				*	cmd,
	int						sync_call
	);


/**
 *	Connect to DDNS server.
 *
 *	@param[in]	context		: the DDNS context.
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] if successfully connected to DDNS
 *				server, then we're ready to send command to server. Otherwise,
 *				an error code will be returned to indicate the error.
 */
static ddns_error peanuthull_connect(struct ddns_context * context);


/**
 *	peanuthull_login
 *
 *	Login to peanuthull DDNS server. The blowfish context will be initialized
 *	here after login succeeded.
 *
 *	@param[in/out]	context	- the DDNS context.
 *
 *	@return	Return [DDNS_ERROR_SUCCESS] if login succeeded and no redirection
 *			is needed. If redirection is needed, [DDNS_ERROR_REDIRECT] will be
 *			returned. Otherwise, it'll return an error code.
 *
 *	@note	If redirection is required, information of the new server will be
 *			saved in field 'redirect_to' of 'peanuthull_context'.
 */
ddns_error peanuthull_login(struct ddns_context *context);


/**
 *	peanuthull_register
 *
 *	Register a domain for update under your DDNS account after logged in. If the
 *	operation succeeded, the following informations will be kept in the context:
 *
 *		1. User ID.
 *		2. Update sequence number used in keep-alive operations.
 *
 *	@param[in/out]	context	- the DDNS context
 *
 *	@return	int		- positive if successful, or -1 if failed.
 */
static ddns_error peanuthull_register(struct ddns_context *context);


/**
 *	peanuthull_request_keepalive
 *
 *	Send keep-alive request to active DDNS server via UDP and update sequence
 *	number according to the server response.
 *
 *	@param[in/out]	context			: DDNS context.
 *	@param[in]		request_type	: keep-alive request request_type
 *
 *	@return		Return 0 if successful, otherwise -1 is returned.
 */
static ddns_error peanuthull_do_keepalive(
	struct ddns_context	*	context,
	unsigned long			request_type
	);


/*============================================================================*
 *	Implementation of Functions
 *============================================================================*/

/**
 *	Create DDNS interface for accessing oray-peanuthull service.
 *
 *	@return		Return pointer to the newly created interface object, or NULL
 *				if there's an error creating interface object.
 */
ddns_interface * peanuthull_interface_create(void)
{
	ddns_interface * ddns = NULL;

	ddns = (ddns_interface*)malloc(sizeof(*ddns));
	if ( NULL != ddns )
	{
		memset(ddns, 0, sizeof(*ddns));

		ddns->initialize		= &peanuthull_interface_initialize;
		ddns->is_ip_changed		= &peanuthull_interface_is_ip_changed;
		ddns->get_ip_address	= &peanuthull_interface_get_ip_address;
		ddns->do_update			= &peanuthull_interface_do_update;
		ddns->finalize			= &peanuthull_interface_finalize;
		ddns->destroy			= &peanuthull_interface_destroy;
	}

	return ddns;
}


/**
 *	Initialize DDNS context for oray-peanuthull service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error peanuthull_interface_initialize(
	struct ddns_context * context
	)
{
	int								redirect_cnt	= 0;
	ddns_error						error_code		= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull		= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	/**
	 *	Step 1: Allocate memory in DDNS context.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 != ddns_create_extra_param(context, sizeof(*peanuthull)) )
		{
			peanuthull = (struct peanuthull_context*)context->extra_data;
			peanuthull->sock = DDNS_INVALID_SOCKET;
		}
		else
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}
	}

	for ( ; redirect_cnt < PEANUTHULL_MAX_REDIRECTION; ++redirect_cnt )
	{
		if ( DDNS_ERROR_REDIRECT == error_code )
		{
			/* handle server redirection, close existing session */
			peanuthull_sendcmd(context, PEANUTHULL_CMD_QUIT, 1);
			if ( (NULL == peanuthull) || (NULL == peanuthull->redirect_to) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
			else
			{
				error_code = DDNS_ERROR_SUCCESS;
			}
		}
		
		if ( DDNS_ERROR_SUCCESS != error_code )
		{
			/* error encountered while logining in */
			break;
		}
		else if ( 0 != context->exit_signal )
		{
			/* exit request is received */
			break;
		}

		/* Step 2: Connect to DDNS server */
		if ( (DDNS_ERROR_SUCCESS == error_code) && (0 == context->exit_signal) )
		{
			error_code = peanuthull_connect(context);

			/* reset redirection information at new tcp session */
			if ( NULL != peanuthull->redirect_to )
			{
				free(peanuthull->redirect_to);
				peanuthull->redirect_to = NULL;
			}
		}

		/* Step 3: Do authentication and get necessary information. */
		if ( (DDNS_ERROR_SUCCESS == error_code) && (0 == context->exit_signal) )
		{
			error_code = peanuthull_login(context);
		}
		
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			/* login succeeded */
			break;
		}
	}
	if ( DDNS_ERROR_REDIRECT == error_code )
	{
		/* maximum redirection count reached */
		error_code = DDNS_ERROR_MAXREDIRECT;
	}

	/* Step 4: Register domain names and initialize blowfish context */
	if ( (DDNS_ERROR_SUCCESS == error_code) && (0 == context->exit_signal) )
	{
		error_code = peanuthull_register(context);
	}

	/* Step 5: Logout via TCP connection. */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		peanuthull_sendcmd(context, PEANUTHULL_CMD_QUIT, 1);
	}

	if ( (NULL != peanuthull) && (DDNS_INVALID_SOCKET != peanuthull->sock) )
	{
		ddns_socket_close(peanuthull->sock);
		peanuthull->sock = DDNS_INVALID_SOCKET;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		peanuthull->binary_mode = 1;
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
static ddns_error peanuthull_interface_is_ip_changed(
	struct ddns_context * context
	)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	ddns_printf_v(	context,
					msg_type_info,
					"Doing keep-alive [%d]... ",
					peanuthull->keep_alive.request.sequence
					);

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( DDNS_INVALID_SOCKET == peanuthull->sock )
		{
			peanuthull->sock = ddns_socket_create(	AF_INET,
													SOCK_DGRAM,
													IPPROTO_UDP
													);
			if ( DDNS_INVALID_SOCKET == peanuthull->sock )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_ulong32 old_address = peanuthull->address;

		error_code = peanuthull_do_keepalive(	context,
												PEANUTHULL_KEEPALIVE_REQ
												);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( (0 == old_address) || (old_address == peanuthull->address) )
			{
				error_code = DDNS_ERROR_NOCHG;
			}
		}

		switch ( error_code )
		{
		case DDNS_ERROR_SUCCESS:
		case DDNS_ERROR_NOCHG:
			peanuthull->failure_cnt = 0;
			ddns_printf_v(	context,
							msg_type_info,
							"successful [IP = %d.%d.%d.%d].\n",
							(int)((peanuthull->address >> 24) & 0xff),
							(int)((peanuthull->address >> 16) & 0xff),
							(int)((peanuthull->address >> 8) & 0xff),
							(int)((peanuthull->address >> 0) & 0xff)
							);
			break;

		case DDNS_ERROR_TIMEOUT:
			++(peanuthull->failure_cnt);
			if ( peanuthull->failure_cnt < 5 )
			{
				error_code = DDNS_ERROR_SUCCESS;
			}
			ddns_printf_v(	context,
							msg_type_info,
							"timeout [count = %d].\n",
							peanuthull->failure_cnt
							);
			break;

		default:
			ddns_printf_v(context, msg_type_info, "failed.\n");
			break;
		}
	}

	return error_code;
}


/**
 *	Get current IP address.
 *
 *	@param[out]	buffer	: buffer to save the IP address.
 *	@param[in]	length	: size of the buffer in characters.
 *
 */
static ddns_error peanuthull_interface_get_ip_address(
	struct ddns_context *	context,
	char				*	buffer,
	size_t					length
	)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 == peanuthull->address )
		{
			error_code = DDNS_ERROR_BADARG;
		}
		else
		{
			size_t real_size = 0;

			real_size = c99_snprintf(	buffer,
										length,
										"%d.%d.%d.%d",
										(int)((peanuthull->address >> 24) & 0xff),
										(int)((peanuthull->address >> 16) & 0xff),
										(int)((peanuthull->address >> 8) & 0xff),
										(int)((peanuthull->address >> 0) & 0xff)
										);
			if ( real_size >= length )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
		}
	}

	return error_code;
}


/**
 *	Update all records via oray-peanuthull service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error peanuthull_interface_do_update(struct ddns_context * context)
{
	/* No need to do it */
	return DDNS_ERROR_SUCCESS;
}


/**
 *	Finalize DDNS context for oray-peanuthull service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error peanuthull_interface_finalize(struct ddns_context * context)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( NULL != peanuthull->redirect_to )
		{
			free(peanuthull->redirect_to);
			peanuthull->redirect_to = NULL;
		}

		/* if we need to inform the server we're going to be off-line */
		if ( 0 != peanuthull->keep_alive.request.session_id )
		{
			if ( DDNS_INVALID_SOCKET == peanuthull->sock )
			{
				peanuthull->sock = ddns_socket_create(	AF_INET,
														SOCK_DGRAM,
														IPPROTO_UDP
														);
				if ( DDNS_INVALID_SOCKET == peanuthull->sock )
				{
					error_code = DDNS_ERROR_UNKNOWN;
				}
			}
		}

		if ( DDNS_INVALID_SOCKET != peanuthull->sock )
		{
			ddns_printf_v(context, msg_type_info, "Stoping keep-alive... ");

			error_code = peanuthull_do_keepalive(	context,
													PEANUTHULL_LOGOUT_REQ
													);
			switch( error_code )
			{
			case DDNS_ERROR_SUCCESS:
				ddns_printf_v(context, msg_type_info, "successful.\n");
				break;
			case DDNS_ERROR_TIMEOUT:
				ddns_printf_v(context, msg_type_info, "timeout.\n");
				break;
			default:
				ddns_printf_v(context, msg_type_info, "failed.\n");
				break;
			}

			ddns_socket_close(peanuthull->sock);
			peanuthull->sock = DDNS_INVALID_SOCKET;
		}

		while ( NULL != peanuthull->domain_list )
		{
			struct ddns_server * next = peanuthull->domain_list->next;

			free(peanuthull->domain_list);
			peanuthull->domain_list = next;
		}

		memset(&(context->active_server), 0, sizeof(context->active_server));

		ddns_remove_extra_param(context);
	}

	return error_code;
}


/**
 *	Destroy DDNS interface object created by [peanuthull_create_interface].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error peanuthull_interface_destroy(ddns_interface* ddns)
{
	ddns_error error_code = DDNS_ERROR_SUCCESS;

	if ( NULL != ddns )
	{
		if ( ddns->destroy == &peanuthull_interface_destroy )
		{
			free(ddns);
			ddns = NULL;
		}
		else if ( NULL != ddns->destroy )
		{
			error_code = ddns->destroy(ddns);
		}
		else
		{
			error_code = DDNS_ERROR_NOTIMPL;
		}
	}

	return error_code;
}


/**
 *	Calculate HMAC authentication digest for peanuthull.
 *
 *	@param[in]	text		: password
 *	@param[in]	text_len	: length of password in bytes
 *	@param[in]	key			: challenge
 *	@param[in]	key_len		: length of challenge in bytes
 *	@param[out]	digest		: buffer for generated digest
 *	@param[in]	digest_len	: size of output buffer in bytes
 *
 *	@return	length of the digest in bytes.
 */
static int peanuthull_hmac(
	unsigned char	*	text,
	size_t				text_len,
	unsigned char	*	key,
	size_t				key_len,
	unsigned char	*	digest,
	size_t				digest_len
	)
{
	size_t len = 0;

	len = hmac_md5(text, text_len, key, key_len, 0, 0) + 2 * sizeof(ddns_ulong32);
	if ( len <= digest_len )
	{
		ddns_ulong32	auth_enc	= 0;
		ddns_ulong32	client_info	= 0;
		ddns_long32		svrtime		= ddns_DL2N(*((ddns_long32*)(key + 6)));
		ddns_long32		shift		= 0;

		/**
		 *	IMPORTANT: [svrtime] and [shift] are signed value, it's possible
		 *	that they're negative.
		 */
		svrtime	= svrtime | (~((ddns_ulong32)PEANUTHULL_AUTH_KEY));
		shift	= svrtime % 30;

		/**
		 *	NOTE:
		 *		The oray peanuthull specification depends on a behavior of the
		 *		IA-32 processors (Intel 286 processor or above):
		 *
		 *		The 8086 does not mask the shift count. However, all other
		 *		IA-32 processors (starting with the Intel 286 processor) do
		 *		mask the shift count to 5 bits, resulting in a maximum count of
		 *		31. This masking is done in all operating modes (including the
		 *		virtual-8086 mode) to reduce the maximum execution time of the
		 *		instructions.
		 */
		auth_enc	= ((ddns_ulong32)svrtime) << ((32 - shift) & 0x0000001F);
		auth_enc	|= ((ddns_ulong32)svrtime) >> (shift & 0x0000001F);

		client_info	= ((PEANUTHULL_CLIENT_ID & 0xFFFF) << 16)
					| (PEANUTHULL_CLIENT_VER & 0xFFFF);

		*((int*)digest) = ddns_DN2L(auth_enc);
		*(((int*)digest)+1) = ddns_DN2L(client_info);

		hmac_md5(	text, text_len,
					key, key_len,
					digest + sizeof(int) * 2, digest_len);
	}

	return len;
}


/**
 *	Initialize peanuthull blowfish context after we get key from server.
 *
 *	@param[in]	ctx		: Blowfish context to be initialized.
 *	@param[in]	key		: Private key to initialize blowfish context.
 *	@param[in]	keyLen	: Length of the private key in bytes.
 */
static void peanuthull_keepalive_init(
	BLOWFISH_CTX	*	ctx,
	unsigned char	*	key,
	size_t				keyLen
	)
{
	/**
	 *	Blowfish S table for peanuthull (modified from standard table).
	 */
	static const unsigned long ORIG_S[4][256] =
	{
		{	0xD1310BA6, 0x98DFB5AC, 0x2FFD72DB, 0xD01ADFB7,
			0xB8E1AFED, 0x6A267E96, 0xBA7C9045, 0xF12C7F99,
			0x24A19947, 0xB3916CF7, 0x0801F2E2, 0x858EFC16,
			0x636920D8, 0x71574E69, 0xA458FEA3, 0xF4933D7E,
			0x0D95748F, 0x728EB658, 0x718BCD58, 0x82154AEE,
			0x7B54A41D, 0xC25A59B5, 0x9C30D539, 0x2AF26013,
			0xC5D1B023, 0x286085F0, 0xCA417918, 0xB8DB38EF,
			0x8E79DCB0, 0x603A180E, 0x6C9E0E8B, 0xB01E8A3E,
			0xD71577C1, 0xBD314B27, 0x78AF2FDA, 0x55605C60,
			0xE65525F3, 0xAA55AB94, 0x57489862, 0x63E81440,
			0x55CA396A, 0x2AAB10B6, 0xB4CC5C34, 0x1141E8CE,
			0xA15486AF, 0x7C72E993, 0xB3EE1411, 0x636FBC2A,
			0x2BA9C55D, 0x741831F6, 0xCE5C3E16, 0x9B87931E,
			0xAFD6BA33, 0x6C24CF5C, 0x7A325381, 0x28958677,
			0x3B8F4898, 0x6B4BB9AF, 0xC4BFE81B, 0x66282193,
			0x61D809CC, 0xFB21A991, 0x487CAC60, 0x5DEC8032,
			0xEF845D5D, 0xE98575B1, 0xDC262302, 0xEB651B88,
			0x23893E81, 0xD396ACC5, 0x0F6D6FF3, 0x83F44239,
			0x2E0B4482, 0xA4842004, 0x69C8F04A, 0x9E1F9B5E,
			0x21C66842, 0xF6E96C9A, 0x670C9C61, 0xABD388F0,
			0x6A51A0D2, 0xD8542F68, 0x960FA728, 0xAB5133A3,
			0x6EEF0B6C, 0x137A3BE4, 0xBA3BF050, 0x7EFB2A98,
			0xA1F1651D, 0x39AF0176, 0x66CA593E, 0x82430E88,
			0x8CEE8619, 0x456F9FB4, 0x7D84A5C3, 0x3B8B5EBE,
			0xE06F75D8, 0x85C12073, 0x401A449F, 0x56C16AA6,
			0x4ED3AA62, 0x363F7706, 0x1BFEDF72, 0x429B023D,
			0x37D0D724, 0xD00A1248, 0xDB0FEAD3, 0x49F1C09B,
			0x075372C9, 0x80991B7B, 0x25D479D8, 0xF6E8DEF7,
			0xE3FE501A, 0xB6794C3B, 0x976CE0BD, 0x04C006BA,
			0xC1A94FB6, 0x409F60C4, 0x5E5C9EC2, 0x196A2463,
			0x68FB6FAF, 0x3E6C53B5, 0x1339B2EB, 0x3B52EC6F,
			0x6DFC511F, 0x9B30952C, 0xCC814544, 0xAF5EBD09,
			0xBEE3D004, 0xDE334AFD, 0x660F2807, 0x192E4BB3,
			0xC0CBA857, 0x45C8740F, 0xD20B5F39, 0xB9D3FBDB,
			0x5579C0BD, 0x1A60320A, 0xD6A100C6, 0x402C7279,
			0x679F25FE, 0xFB1FA3CC, 0x8EA5E9F8, 0xDB3222F8,
			0x3C7516DF, 0xFD616B15, 0x2F501EC8, 0xAD0552AB,
			0x323DB5FA, 0xFD238760, 0x53317B48, 0x3E00DF82,
			0x9E5C57BB, 0xCA6F8CA0, 0x1A87562E, 0xDF1769DB,
			0xD542A8F6, 0x287EFFC3, 0xAC6732C6, 0x8C4F5573,
			0x695B27B0, 0xBBCA58C8, 0xE1FFA35D, 0xB8F011A0,
			0x10FA3D98, 0xFD2183B8, 0x4AFCB56C, 0x2DD1D35B,
			0x9A53E479, 0xB6F84565, 0xD28E49BC, 0x4BFB9790,
			0xE1DDF2DA, 0xA4CB7E33, 0x62FB1341, 0xCEE4C6E8,
			0xEF20CADA, 0x36774C01, 0xD07E9EFE, 0x2BF11FB4,
			0x95DBDA4D, 0xAE909198, 0xEAAD8E71, 0x6B93D5A0,
			0xD08ED1D0, 0xAFC725E0, 0x8E3C5B2F, 0x8E7594B7,
			0x8FF6E2FB, 0xF2122B64, 0x8888B812, 0x900DF01C,
			0x4FAD5EA0, 0x688FC31C, 0xD1CFF191, 0xB3A8C1AD,
			0x2F2F2218, 0xBE0E1777, 0xEA752DFE, 0x8B021FA1,
			0xE5A0CC0F, 0xB56F74E8, 0x18ACF3D6, 0xCE89E299,
			0xB4A84FE0, 0xFD13E0B7, 0x7CC43B81, 0xD2ADA8D9,
			0x165FA266, 0x80957705, 0x93CC7314, 0x211A1477,
			0xE6AD2065, 0x77B5FA86, 0xC75442F5, 0xFB9D35CF,
			0xEBCDAF0C, 0x7B3E89A0, 0xD6411BD3, 0xAE1E7E49,
			0x00250E2D, 0x2071B35E, 0x226800BB, 0x57B8E0AF,
			0x2464369B, 0xF009B91E, 0x5563911D, 0x56DFA6AA,
			0x78C14389, 0xD95A537F, 0x207D5BA2, 0x02E5B9C5,
			0x83260376, 0x6295CFA9, 0x11C81968, 0x4E734A41,
			0xB3472DCA, 0x7B14A94A, 0x1B510052, 0x9A532915,
			0xD60F573F, 0xBC9BC6E4, 0x2B60A476, 0x81E67400,
			0x08BA6FB5, 0x571BE91F, 0xF296EC6B, 0x2A0DD915,
			0xB6636521, 0xE7B9F9B6, 0xFF340528, 0xC5855664,
			0x53B02D5D, 0xA99F8FA1, 0x08BA4799, 0x6E85076A	},
		{	0x4B7A70E9, 0xB5B32944, 0xDB75092E, 0xC4192623,
			0xAD6EA6B0, 0x49A7DF7D, 0x9CEE60B8, 0x8FEDB266,
			0xECAA8C71, 0x699A17FF, 0x5664526C, 0xC2B19EE1,
			0x193602A5, 0x75094C29, 0xA0591340, 0xE4183A3E,
			0x3F54989A, 0x5B429D65, 0x6B8FE4D6, 0x99F73FD6,
			0xA1D29C07, 0xEFE830F5, 0x4D2D38E6, 0xF0255DC1,
			0x4CDD2086, 0x8470EB26, 0x6382E9C6, 0x021ECC5E,
			0x09686B3F, 0x3EBAEFC9, 0x3C971814, 0x6B6A70A1,
			0x687F3584, 0x52A0E286, 0xB79C5305, 0xAA500737,
			0x3E07841C, 0x7FDEAE5C, 0x8E7D44EC, 0x5716F2B8,
			0xB03ADA37, 0xF0500C0D, 0xF01C1F04, 0x0200B3FF,
			0xAE0CF51A, 0x3CB574B2, 0x25837A58, 0xDC0921BD,
			0xD19113F9, 0x7CA92FF6, 0x94324773, 0x22F54701,
			0x3AE5E581, 0x37C2DADC, 0xC8B57634, 0x9AF3DDA7,
			0xA9446146, 0x0FD0030E, 0xECC8C73E, 0xA4751E41,
			0xE238CD99, 0x3BEA0E2F, 0x3280BBA1, 0x183EB331,
			0x4E548B38, 0x4F6DB908, 0x6F420D03, 0xF60A04BF,
			0x2CB81290, 0x24977C79, 0x5679B072, 0xBCAF89AF,
			0xDE9A771F, 0xD9930810, 0xB38BAE12, 0xDCCF3F2E,
			0x5512721F, 0x2E6B7124, 0x501ADDE6, 0x9F84CD87,
			0x7A584718, 0x7408DA17, 0xBC9F9ABC, 0xE94B7D8C,
			0xEC7AEC3A, 0xDB851DFA, 0x63094366, 0xC464C3D2,
			0xEF1C1847, 0x3215D908, 0xDD433B37, 0x24C2BA16,
			0x12A14D43, 0x2A65C451, 0x50940002, 0x133AE4DD,
			0x71DFF89E, 0x10314E55, 0x81AC77D6, 0x5F11199B,
			0x043556F1, 0xD7A3C76B, 0x3C11183B, 0x5924A509,
			0xF28FE6ED, 0x97F1FBFA, 0x9EBABF2C, 0x1E153C6E,
			0x86E34570, 0xEAE96FB1, 0x860E5E0A, 0x5A3E2AB3,
			0x771FE71C, 0x4E3D06FA, 0x2965DCB9, 0x99E71D0F,
			0x803E89D6, 0x5266C825, 0x2E4CC978, 0x9C10B36A,
			0xC6150EBA, 0x94E2EA78, 0xA5FC3C53, 0x1E0A2DF4,
			0xF2F74EA7, 0x361D2B3D, 0x1939260F, 0x19C27960,
			0x5223A708, 0xF71312B6, 0xEBADFE6E, 0xEAC31F66,
			0xE3BC4595, 0xA67BC883, 0xB17F37D1, 0x018CFF28,
			0xC332DDEF, 0xBE6C5AA5, 0x65582185, 0x68AB9802,
			0xEECEA50F, 0xDB2F953B, 0x2AEF7DAD, 0x5B6E2F84,
			0x1521B628, 0x29076170, 0xECDD4775, 0x619F1510,
			0x13CCA830, 0xEB61BD96, 0x0334FE1E, 0xAA0363CF,
			0xB5735C90, 0x4C70A239, 0xD59E9E0B, 0xCBAADE14,
			0xEECC86BC, 0x60622CA7, 0x9CAB5CAB, 0xB2F3846E,
			0x648B1EAF, 0x19BDF0CA, 0xA02369B9, 0x655ABB50,
			0x40685A32, 0x3C2AB4B3, 0x319EE9D5, 0xC021B8F7,
			0x9B540B19, 0x875FA099, 0x95F7997E, 0x623D7DA8,
			0xF837889A, 0x97E32D77, 0x11ED935F, 0x16681281,
			0x0E358829, 0xC7E61FD6, 0x96DEDFA1, 0x7858BA99,
			0x57F584A5, 0x1B227263, 0x9B83C3FF, 0x1AC24696,
			0xCDB30AEB, 0x532E3054, 0x8FD948E4, 0x6DBC3128,
			0x58EBF2EF, 0x34C6FFEA, 0xFE28ED61, 0xEE7C3C73,
			0x5D4A14D9, 0xE864B7E3, 0x42105D14, 0x203E13E0,
			0x45EEE2B6, 0xA3AAABEA, 0xDB6C4F15, 0xFACB4FD0,
			0xC742F442, 0xEF6ABBB5, 0x654F3B1D, 0x41CD2105,
			0xD81E799E, 0x86854DC7, 0xE44B476A, 0x3D816250,
			0xCF62A1F2, 0x5B8D2646, 0xFC8883A0, 0xC1C7B6A3,
			0x7F1524C3, 0x69CB7492, 0x47848A0B, 0x5692B285,
			0x095BBF00, 0xAD19489D, 0x1462B174, 0x23820E00,
			0x58428D2A, 0x0C55F5EA, 0x1DADF43E, 0x233F7061,
			0x3372F092, 0x8D937E41, 0xD65FECF1, 0x6C223BDB,
			0x7CDE3759, 0xCBEE7460, 0x4085F2A7, 0xCE77326E,
			0xA6078084, 0x19F8509E, 0xE8EFD855, 0x61D99735,
			0xA969A7AA, 0xC50C06C2, 0x5A04ABFC, 0x800BCADC,
			0x9E447A2E, 0xC3453484, 0xFDD56705, 0x0E1E9EC9,
			0xDB73DBD3, 0x105588CD, 0x675FDA79, 0xE3674340,
			0xC5C43465, 0x713E38D8, 0x3D28F89E, 0xF16DFF20,
			0x153E21E7, 0x8FB03D4A, 0xE6E39F2B, 0xDB83ADF7	},
		{	0xE93D5A68, 0x948140F7, 0xF64C261C, 0x94692934,
			0x411520F7, 0x7602D4F7, 0xBCF46B2E, 0xD4A20068,
			0xD4082471, 0x3320F46A, 0x43B7D4B7, 0x500061AF,
			0x1E39F62E, 0x97244546, 0x14214F74, 0xBF8B8840,
			0x4D95FC1D, 0x96B591AF, 0x70F4DDD3, 0x66A02F45,
			0xBFBC09EC, 0x03BD9785, 0x7FAC6DD0, 0x31CB8504,
			0x96EB27B3, 0x55FA3941, 0xDA2547E6, 0xABCA0A9A,
			0x28507825, 0x530429F4, 0x0A2C86DA, 0xE9B66DFB,
			0x68DC1462, 0xD7486900, 0x680EC0A4, 0x27A18DEE,
			0x4F3FFEA2, 0xE887AD8C, 0xB58CE006, 0x7AF4D6B6,
			0xAACE1E7C, 0xD3375FEC, 0xCE78A399, 0x406B2A42,
			0x20FE9E35, 0xD9F385B9, 0xEE39D7AB, 0x3B124E8B,
			0x1DC9FAF7, 0x4B6D1856, 0x26A36631, 0xEAE397B2,
			0x3A6EFA74, 0xDD5B4332, 0x6841E7F7, 0xCA7820FB,
			0xFB0AF54E, 0xD8FEB397, 0x454056AC, 0xBA489527,
			0x55533A3A, 0x20838D87, 0xFE6BA9B7, 0xD096954B,
			0x55A867BC, 0xA1159A58, 0xCCA92963, 0x99E1DB33,
			0xA62A4A56, 0x3F3125F9, 0x5EF47E1C, 0x9029317C,
			0xFDF8E802, 0x04272F70, 0x80BB155C, 0x05282CE3,
			0x95C11548, 0xE4C66D22, 0x48C1133F, 0xC70F86DC,
			0x07F9C9EE, 0x41041F0F, 0x404779A4, 0x5D886E17,
			0x325F51EB, 0xD59BC0D1, 0xF2BCC18F, 0x41113564,
			0x257B7834, 0x602A9C60, 0xDFF8E8A3, 0x1F636C1B,
			0x0E12B4C2, 0x02E1329E, 0xAF664FD1, 0xCAD18115,
			0x6B2395E0, 0x333E92E1, 0x3B240B62, 0xEEBEB922,
			0x85B2A20E, 0xE6BA0D99, 0xDE720C8C, 0x2DA2F728,
			0xD0127845, 0x95B794FD, 0x647D0862, 0xE7CCF5F0,
			0x5449A36F, 0x877D48FA, 0xC39DFD27, 0xF33E8D1E,
			0x0A476341, 0x992EFF74, 0x3A6F6EAB, 0xF4F8FD37,
			0xA812DC60, 0xA1EBDDF8, 0x991BE14C, 0xDB6E6B0D,
			0xC67B5510, 0x6D672C37, 0x2765D43B, 0xDCD0E804,
			0xF1290DC7, 0xCC00FFA3, 0xB5390F92, 0x690FED0B,
			0x667B9FFB, 0xCEDB7D9C, 0xA091CF0B, 0xD9155EA3,
			0xBB132F88, 0x515BAD24, 0x7B9479BF, 0x763BD6EB,
			0x37392EB3, 0xCC115979, 0x8026E297, 0xF42E312D,
			0x6842ADA7, 0xC66A2B3B, 0x12754CCC, 0x782EF11C,
			0x6A124237, 0xB79251E7, 0x06A1BBE6, 0x4BFB6350,
			0x1A6B1018, 0x11CAEDFA, 0x3D25BDD8, 0xE2E1C3C9,
			0x44421659, 0x0A121386, 0xD90CEC6E, 0xD5ABEA2A,
			0x64AF674E, 0xDA86A85F, 0xBEBFE988, 0x64E4C3FE,
			0x9DBC8057, 0xF0F7C086, 0x60787BF8, 0x6003604D,
			0xD1FD8346, 0xF6381FB0, 0x7745AE04, 0xD736FCCC,
			0x83426B33, 0xF01EAB71, 0xB0804187, 0x3C005E5F,
			0x77A057BE, 0xBDE8AE24, 0x55464299, 0xBF582E61,
			0x4E58F48F, 0xF2DDFDA2, 0xF474EF38, 0x8789BDC2,
			0x5366F9C3, 0xC8B38E74, 0xB475F255, 0x46FCD9B9,
			0x7AEB2661, 0x8B1DDF84, 0x846A0E79, 0x915F958E,
			0x466E598E, 0x20B45770, 0x8CD55591, 0xC902DE4C,
			0xB90BACE1, 0xBB8205D0, 0x11A86248, 0x7574A99E,
			0xB77F19B6, 0xE0A9DC09, 0x662D09A1, 0xC4324633,
			0xE85A1F02, 0x09F0BE8C, 0x4A99A025, 0x1D6EFE10,
			0x1AB93D1D, 0x0BA5A4DF, 0xA186F20F, 0x2868F169,
			0xDCB7DA83, 0x573906FE, 0xA1E2CE9B, 0x4FCD7F52,
			0x50115E01, 0xA70683FA, 0xA002B5C4, 0x0DE6D027,
			0x9AF88C27, 0x773F8641, 0xC3604C06, 0x61A806B5,
			0xF0177A28, 0xC0F586E0, 0x006058AA, 0x30DC7D62,
			0x11E69ED7, 0x2338EA63, 0x53C2DD94, 0xC2C21634,
			0xBBCBEE56, 0x90BCB6DE, 0xEBFC7DA1, 0xCE591D76,
			0x6F05E409, 0x4B7C0188, 0x39720A3D, 0x7C927C24,
			0x86E3725F, 0x724D9DB9, 0x1AC15BB4, 0xD39EB8FC,
			0xED545578, 0x08FCA5B5, 0xD83D7CD3, 0x4DAD0FC4,
			0x1E50EF5E, 0xB161E6F8, 0xA28514D9, 0x6C51133C,
			0x6FD5C7E7, 0x56E14EC4, 0x362ABFCE, 0xDDC6C837,
			0xD79A3234, 0x92638212, 0x670EFA8E, 0x406000E0	},
		{	0x3A39CE37, 0xD3FAF5CF, 0xABC27737, 0x5AC52D1B,
			0x5CB0679E, 0x4FA33742, 0xD3822740, 0x99BC9BBE,
			0xD5118E9D, 0xBF0F7315, 0xD62D1C7E, 0xC700C47B,
			0xB78C1B6B, 0x21A19045, 0xB26EB1BE, 0x6A366EB4,
			0x5748AB2F, 0xBC946E79, 0xC6A376D2, 0x6549C2C8,
			0x530FF8EE, 0x468DDE7D, 0xD5730A1D, 0x42D04DC6,
			0x2939BBDB, 0xA9BA4650, 0xAC9526E8, 0xBE5EE304,
			0xA1FAD5F0, 0x6A2D519A, 0x63EF8CE2, 0x9A86EE22,
			0xC089C2B8, 0x43242EF6, 0xA51E03AA, 0x9CF2D0A4,
			0x83C061BA, 0x9BE96A4D, 0x8FE51550, 0xBA645BD6,
			0x2826A2F9, 0xA73A3AE1, 0x4BA99586, 0xEF5562E9,
			0xC72FEFD3, 0xF752F7DA, 0x3F046F69, 0x77FA0A59,
			0x80E4A915, 0x87B08601, 0x9B09E6AD, 0x3B3EE593,
			0xE990FD5A, 0x9E34D797, 0x2CF0B7D9, 0x022B8B51,
			0x96D5AC3A, 0x017DA67D, 0xD1CF3ED6, 0x7C7D2D28,
			0x1F9F25CF, 0xADF2B89B, 0x5AD6B472, 0x5A88F54C,
			0xE029AC71, 0xE019A5E6, 0x47B0ACFD, 0xED93FA9B,
			0xE8D3C48D, 0x283B57CC, 0xF8D56629, 0x79132E28,
			0x785F0191, 0xED756055, 0xF7960E44, 0xE3D35E8C,
			0x15056DD4, 0x88F46DBA, 0x03A16125, 0x0564F0BD,
			0xC3EB9E15, 0x3C9057A2, 0x97271AEC, 0xA93A072A,
			0x1B3F6D9B, 0x1E6321F5, 0xF59C66FB, 0x26DCF319,
			0x7533D928, 0xB155FDF5, 0x03563482, 0x8ABA3CBB,
			0x28517711, 0xC20AD9F8, 0xABCC5167, 0xCCAD925F,
			0x4DE81751, 0x3830DC8E, 0x379D5862, 0x9320F991,
			0xEA7A90C2, 0xFB3E7BCE, 0x5121CE64, 0x774FBE32,
			0xA8B6E37E, 0xC3293D46, 0x48DE5369, 0x6413E680,
			0xA2AE0810, 0xDD6DB224, 0x69852DFD, 0x09072166,
			0xB39A460A, 0x6445C0DD, 0x586CDECF, 0x1C20C8AE,
			0x5BBEF7DD, 0x1B588D40, 0xCCD2017F, 0x6BB4E3BB,
			0xDDA26A7E, 0x3A59FF45, 0x3E350A44, 0xBCB4CDD5,
			0x72EACEA8, 0xFA6484BB, 0x8D6612AE, 0xBF3C6F47,
			0xD29BE463, 0x542F5D9E, 0xAEC2771B, 0xF64E6370,
			0x740E0D8D, 0xE75B1357, 0xF8721671, 0xAF537D5D,
			0x4040CB08, 0x4EB4E2CC, 0x34D2466A, 0x0115AF84,
			0xE1B00428, 0x95983A1D, 0x06B89FB4, 0xCE6EA048,
			0x6F3F3B82, 0x3520AB82, 0x011A1D4B, 0x277227F8,
			0x611560B1, 0xE7933FDC, 0xBB3A792B, 0x344525BD,
			0xA08839E1, 0x51CE794B, 0x2F32C9B7, 0xA01FBAC9,
			0xE01CC87E, 0xBCC7D1F6, 0xCF0111C3, 0xA1E8AAC7,
			0x1A908749, 0xD44FBD9A, 0xD0DADECB, 0xD50ADA38,
			0x0339C32A, 0xC6913667, 0x8DF9317C, 0xE0B12B4F,
			0xF79E59B7, 0x43F5BB3A, 0xF2D519FF, 0x27D9459C,
			0xBF97222C, 0x15E6FC2A, 0x0F91FC71, 0x9B941525,
			0xFAE59361, 0xCEB69CEB, 0xC2A86459, 0x12BAA8D1,
			0xB6C1075E, 0xE3056A0C, 0x10D25065, 0xCB03A442,
			0xE0EC6E0E, 0x1698DB3B, 0x4C98A0BE, 0x3278E964,
			0x9F1F9532, 0xE0D392DF, 0xD3A0342B, 0x8971F21E,
			0x1B0A7441, 0x4BA3348C, 0xC5BE7120, 0xC37632D8,
			0xDF359F8D, 0x9B992F2E, 0xE60B6F47, 0x0FE3F11D,
			0xE54CDA54, 0x1EDAD891, 0xCE6279CF, 0xCD3E7E6F,
			0x1618B166, 0xFD2C1D05, 0x848FD2C5, 0xF6FB2299,
			0xF523F357, 0xA6327623, 0x93A83531, 0x56CCCD02,
			0xACF08162, 0x5A75EBB5, 0x6E163697, 0x88D273CC,
			0xDE966292, 0x81B949D0, 0x4C50901B, 0x71C65614,
			0xE6C6C7BD, 0x327A140A, 0x45E1D006, 0xC3F27B9A,
			0xC9AA53FD, 0x62A80F00, 0xBB25BFE2, 0x35BDD2F6,
			0x71126905, 0xB2040222, 0xB6CBCF7C, 0xCD769C2B,
			0x53113EC0, 0x1640E3D3, 0x38ABBD60, 0x2547ADF0,
			0xBA38209C, 0xF746CE76, 0x77AFA1C5, 0x20756060,
			0x85CBFE4E, 0x8AE88DD8, 0x7AAAF9B0, 0x4CF9AA7E,
			0x1948C25C, 0x02FB8A8C, 0x01C36AE4, 0xD6EBE1F9,
			0x90D4F869, 0xA65CDEA0, 0x3F09252D, 0xC208E69F,
			0xB74E6132, 0xCE77E25B, 0x578FDFE3, 0x3AC372E6	},
	};

	ddns_ulong32	data	= 0;
	ddns_ulong32	dataL	= 0;
	ddns_ulong32	dataR	= 0;
	size_t			i		= 0;
	size_t			j		= 0;
	size_t			k		= 0;

	for ( i = 0; i < sizeof(ctx->S) / sizeof(ctx->S[0]); ++i )
	{
		for ( j = 0; j < sizeof(ctx->S[0]) / sizeof(ctx->S[0][0]); ++j )
		{
			ctx->S[i][j] = ORIG_S[i][j];
		}
	}

	for ( i = 0; i < sizeof(ctx->P) / sizeof(ctx->P[0]); ++i )
	{
		ctx->P[i] = 0;
	}

	for ( i = 0, j = 0; i < sizeof(ctx->P) / sizeof(ctx->P[0]); ++i )
	{
		data = 0;
		for ( k = 0; k < 4; ++k )
		{
			data = (data << 8) | key[j];
			if ( (++j) >= keyLen )
			{
				j = 0;
			}
		}

		ctx->P[i] = ctx->P[i] ^ data;
	}

	dataL = 0;
	dataR = 0;
	for ( i = 0; i < sizeof(ctx->P) / sizeof(ctx->P[0]); i += 2 )
	{
		Blowfish_Encrypt(ctx, &dataL, &dataR);

		ctx->P[i] = dataL;
		ctx->P[i + 1] = dataR;
	}

	for ( i = 0; i < sizeof(ctx->S) / sizeof(ctx->S[0]); ++i )
	{
		for ( j = 0; j < sizeof(ctx->S[0]) / sizeof(ctx->S[0][0]); j += 2 )
		{
			Blowfish_Encrypt(ctx, &dataL, &dataR);

			ctx->S[i][j] = dataL;
			ctx->S[i][j + 1] = dataR;
		}
	}
}


/**
 *	Receive a message from DDNS server.
 *
 *	@param[in]	context			: DDNS context
 *	@param[out]	buffer			: buffer to store response from server
 *	@param[in]	buffer_len		: size of the buffer in bytes
 *	@param[out]	error_code		: the error code if -1 is returned.
 *
 *	@return If successful, it will return count of characters received from
 *			server excluding null terminator. Otherwise, -1 will be returned
 *			to indicate an error.
 */
static int peanuthull_recv(
   struct ddns_context	*	context,
   char					*	buffer,
   int						buffer_len,
   ddns_error			*	error_code
   )
{
	int								recv_bytes	= 0;
	ddns_error						status_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		status_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			status_code = DDNS_ERROR_BADARG;
		}
	}

_RETRY:
	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		fd_set			fd;
		struct timeval	tv = { 0, 0 };

		FD_ZERO(&fd);
		FD_SET(peanuthull->sock, &fd);
		tv.tv_sec = context->timeout;

		/* wait until the socket is read for read */
		switch ( select(peanuthull->sock + 1, &fd, 0, 0, &tv) )
		{
		case 0:
			/* timeout */
			status_code = DDNS_ERROR_TIMEOUT;
			break;

		case 1:
			/* socket is read for read */
			break;

		default:
			/* unknown error */
			status_code = DDNS_ERROR_UNKNOWN;
			break;
		}
	}

	if ( (DDNS_ERROR_SUCCESS == status_code) && (0 != peanuthull->binary_mode) )
	{
		recv_bytes = recv(peanuthull->sock, buffer, buffer_len, 0);
		switch( recv_bytes )
		{
		case -1:
			/* error occurred */
			switch ( ddns_socket_get_errno() )
			{
			case ECONNRESET:
				status_code = DDNS_ERROR_CONNRST;
				break;

			case EWOULDBLOCK:
				/* haven't received all data yet, retry. */
				goto _RETRY;
				break;

			case EMSGSIZE:
				status_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
				break;

			default:
				status_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		case 0:
			/* connection is closed */
			status_code = DDNS_ERROR_CONNCLOSE;
			break;

		default:
			break;
		}
	}
	if ( (DDNS_ERROR_SUCCESS == status_code) && (0 == peanuthull->binary_mode) )
	{
		int		hit_end	= 0;
		while ( (0 == hit_end) && (recv_bytes + 1) < buffer_len )
		{
			char chr = 0;

			switch( recv(peanuthull->sock, &chr, 1, 0) )
			{
			case 1:
				if ( '\n' == chr )
				{
					/* hit line-feed, stop */
					hit_end = 1;

					/* collapse "\r\n" to "\n" */
					if ( (recv_bytes > 0) && ('\r' == buffer[recv_bytes - 1]) )
					{
						--recv_bytes;
					}

					/* strip line-feed */
					buffer[recv_bytes] = '\0';
				}
				else
				{
					buffer[recv_bytes] = chr;
					++recv_bytes;
				}
				break;

			case 0:
				/* connection is closed, stop */
				hit_end = 1;
				status_code = DDNS_ERROR_CONNCLOSE;
				break;

			default:
				/* error occurred, stop */
				hit_end = 1;
				switch ( ddns_socket_get_errno() )
				{
				case ECONNRESET:
					status_code = DDNS_ERROR_CONNRST;
					break;

				case EWOULDBLOCK:
					/* haven't received all data yet, retry. */
					goto _RETRY;
					break;

				default:
					status_code = DDNS_ERROR_UNKNOWN;
					break;
				}
				break;
			}
		}

		if ( (DDNS_ERROR_SUCCESS == status_code) && (0 == hit_end) )
		{
			status_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( NULL != error_code )
	{
		(*error_code) = status_code;
	}

	if ( (DDNS_ERROR_SUCCESS == status_code) && (0 == peanuthull->binary_mode) )
	{
		ddns_printf_v(context, msg_type_info, "S: %s\n", buffer);
	}

	return (DDNS_ERROR_SUCCESS == status_code ? recv_bytes : -1);
}


/**
 *	Send a command to DDNS server.
 *
 *	@param[in]	sock		- socket to send command
 *	@param[in]	cmd			- the command to send to server
 *	@param[in]	sync_call	- whether to wait for response from server
 *
 *	@return	Return [DDNS_ERROR_SUCCESS] if the command is successfully sent to
 *			server. Otherwise an error code will be returned.
 *
 *	@note	If 'cmd' is PEANUTHULL_CMD_QUIT, the tcp connection will be closed
 *			before returning.
 */
static ddns_error peanuthull_sendcmd(
	struct ddns_context *	context,
	char				*	cmd,
	int						sync_call
	)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		fd_set			fd;
		struct timeval	tv =  { 0, 0 };

		FD_ZERO(&fd);
		FD_SET(peanuthull->sock, &fd);
		tv.tv_sec = context->timeout;

		/* wait until the socket is read for write */
		switch ( select(peanuthull->sock + 1, 0, &fd, 0, &tv) )
		{
		case 0:
			/* timeout */
			error_code = DDNS_ERROR_TIMEOUT;
			break;
		case 1:
			/* socket is ready for write */
			break;
		default:
			/* unknown error */
			error_code = DDNS_ERROR_UNKNOWN;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int			cmd_len	= 0;
		char	*	command	= NULL;

		cmd_len = strlen(cmd) + sizeof("\r\n");
		command = malloc(cmd_len * sizeof(command[0]));
		if ( NULL == command )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			int		sent_len = 0;

			c99_snprintf(command, cmd_len, "%s\r\n", cmd);
			ddns_printf_v(context, msg_type_info, "C: %s", command);

			sent_len = send(peanuthull->sock, command, cmd_len, 0);
			if ( -1 == sent_len )
			{
				/* error in sending command to server */
				if ( ECONNRESET == ddns_socket_get_errno() )
				{
					error_code = DDNS_ERROR_CONNRST;
				}
				else
				{
					error_code = DDNS_ERROR_CONNECTION;
				}
			}
			else if ( sent_len != cmd_len )
			{
				/* part of the command is sent, treat as error */
				error_code = DDNS_ERROR_CONNECTION;
			}
			else
			{
				/* command is successfully sent to server. */
			}
		}

		if ( NULL != command )
		{
			free(command);
			command = NULL;
		}
	}

	if ( (DDNS_ERROR_SUCCESS == error_code) && (0 != sync_call) )
	{
		int length = peanuthull_recv(	context,
										peanuthull->buffer,
										sizeof(peanuthull->buffer) - 1,
										&error_code
										);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( (-1 == length) || (length >= sizeof(peanuthull->buffer)) )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}
	}

	if ( 0 == ddns_strcasecmp(PEANUTHULL_CMD_QUIT, cmd) )
	{
		/* special operation for "QUIT" command: close tcp connection */
		if ( DDNS_INVALID_SOCKET != peanuthull->sock )
		{
			ddns_socket_close(peanuthull->sock);
			peanuthull->sock = DDNS_INVALID_SOCKET;
		}
	}

	return error_code;
}


/**
 *	Connect to DDNS server.
 *
 *	@param[in]	context		: the DDNS context.
 *
 *	@return		Return [DDNS_ERROR_SUCCESS] if successfully connected to DDNS
 *				server, then we're ready to send command to server. Otherwise,
 *				an error code will be returned to indicate the error.
 */
static ddns_error peanuthull_connect(struct ddns_context * context)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull	= NULL;

	/* Step 1: argument validity check */
	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	/* Step 2: connect to DDNS server via TCP */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		struct ddns_server * server = NULL;
		if ( NULL == peanuthull->redirect_to )
		{
			server = context->server;
		}
		else
		{
			server = peanuthull->redirect_to;
		}

		peanuthull->sock = ddns_socket_create_tcp(	server->domain,
													server->port,
													context->timeout,
													&(context->exit_signal)
													);
		if ( DDNS_INVALID_SOCKET != peanuthull->sock )
		{
			int			result		= 0;
			socklen_t	addr_size	= sizeof(context->active_server);

			ddns_printf_v(context,	msg_type_info,
									"Connected to \"%s:%d\".\n",
									server->domain,
									server->port
									);

			result = getpeername(	peanuthull->sock,
									(struct sockaddr*)&context->active_server,
									&addr_size
									);
			if ( -1 == result )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}
		else
		{
			if ( ENETUNREACH == ddns_socket_get_errno() )
			{
				error_code = DDNS_ERROR_UNREACHABLE;
			}
			else
			{
				error_code = DDNS_ERROR_TIMEOUT;
			}
		}
	}

	/* Step 3: Wait for 'welcome' string. */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int length = peanuthull_recv(	context,
										peanuthull->buffer,
										sizeof(peanuthull->buffer),
										&error_code
										);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( (-1 == length) || (length >= sizeof(peanuthull->buffer)) )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}
	}

	return error_code;
}


/**
 *	Login to peanuthull DDNS server. The blowfish context will be initialized
 *	here after login succeeded.
 *
 *	@param[in/out]	context	- the DDNS context.
 *
 *	@return	Return [DDNS_ERROR_SUCCESS] if login succeeded and no redirection
 *			is needed. If redirection is needed, [DDNS_ERROR_REDIRECT] will be
 *			returned. Otherwise, it'll return an error code.
 *
 *	@note	If redirection is required, information of the new server will be
 *			saved in field 'redirect_to' of 'peanuthull_context'.
 */
ddns_error peanuthull_login(struct ddns_context *context)
{
	ddns_error						error_code		= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull		= NULL;
	unsigned char				*	challenge		= NULL;
	int								challenge_len	= 0;

	/**
	 *	Step 1: arguments validity check.
	 */
	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	/**
	 *	Step 2: send authentication request to server and get authentication
	 *			challenge from server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = peanuthull_sendcmd(context, PEANUTHULL_CMD_AUTH, 1);

		if ( (DDNS_ERROR_SUCCESS == error_code) )
		{
			/* server message of "334 ...." is expected */
			if ( 0 != memcmp("334", peanuthull->buffer, 3) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	/**
	 *	Step 3: send authentication digest to server and get basic account
	 *			information from server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int			digest_len		= 0;
		char	*	command			= NULL;
		int			command_len		= 0;
		char	*	command64		= NULL;
		int			command64_len	= 0;
		int			username_len	= strlen(context->username);
		int			password_len	= strlen(context->password);

		/* calculate the size of decoded challenge code */
		challenge_len = base64_decode(&(peanuthull->buffer[4]), NULL, 0);
		if ( -1 == challenge_len )
		{
			error_code = DDNS_ERROR_BADSVR;
		}

		/* allocate memory for storing challenge code */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			challenge = (unsigned char*)malloc(challenge_len + 1);
			if ( NULL != challenge )
			{
				memset(challenge, 0, challenge_len + 1);
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
			}
		}

		/* decode challenge code */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			challenge_len = base64_decode(	&(peanuthull->buffer[4]),
											challenge,
											challenge_len
											);
			if ( -1 == challenge_len )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}

		/* calculate the size of digest */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			digest_len = peanuthull_hmac(	(unsigned char*)context->password,
											password_len,
											challenge,
											challenge_len,
											0,
											0
											);
			if ( -1 == digest_len )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}

		/* allocate memory for storing command */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			command_len = username_len + 1 + digest_len;
			command = malloc((command_len + 1) * sizeof(command[0]));
			if ( NULL != command )
			{
				memset(command, 0, (command_len + 1) * sizeof(command[0]));
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
			}
		}

		/* full command line header (exclude password digest) */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			int length = c99_snprintf(	command,
										command_len + 1,
										"%s ",
										context->username
										);
			if ( length > username_len + 1 )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
		}

		/* calculate the digest */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			unsigned char * digest = (unsigned char*)command + username_len + 1;
			digest_len = peanuthull_hmac(	(unsigned char*)context->password,
											password_len,
											challenge,
											challenge_len,
											digest,
											digest_len
											);
			if ( -1 == digest_len )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}

		/* calculate the size of base64 encoded command line */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			command64_len = base64_encode(	(unsigned char*)command,
											command_len,
											NULL,
											0
											);
			if ( -1 == command64_len )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}

		/* allocate memory for storing base64 encoded command line */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			command64 = malloc(command64_len + 1);
			if ( NULL != command64 )
			{
				memset(command64, 0, command64_len + 1);
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
			}
		}

		/* encode command line */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			memset(command64, 0, command64_len + 1);
			command64_len = base64_encode(	(unsigned char*)command,
											command_len,
											command64,
											command64_len
											);
			if ( -1 == command64_len )
			{
				error_code = DDNS_ERROR_UNKNOWN;
			}
		}

		/* send encoded command to server */
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			error_code = peanuthull_sendcmd(context, command64, 1);
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			/* server message of "250 ..." or "536 ..." are expected */
			if ( 0 == memcmp("536 ", peanuthull->buffer, 4) )
			{
				char				*	start_pos	= NULL;
				char				*	end_pos		= NULL;
				struct ddns_server	*	server		= NULL;

				start_pos	= strchr(peanuthull->buffer, '<');
				end_pos		= strchr(peanuthull->buffer, '>');
				if ( (NULL != start_pos) && (end_pos > start_pos) )
				{
					if ( end_pos - start_pos > sizeof(server->domain) )
					{
						error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
					}
				}
				else
				{
					error_code = DDNS_ERROR_BADSVR;
				}

				if ( DDNS_ERROR_SUCCESS == error_code )
				{
					server = malloc(sizeof(struct ddns_server));
					if ( NULL == server )
					{
						error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
					}
				}

				if ( DDNS_ERROR_SUCCESS == error_code )
				{
					error_code		= DDNS_ERROR_REDIRECT;
					server->port	= 6060;
					server->next	= NULL;
					c99_strncpy(server->domain, start_pos + 1, end_pos - start_pos);
					peanuthull->redirect_to = server;
				}
			}
			else if ( 0 != memcmp("250 ", peanuthull->buffer, 4) )
			{
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADAUTH);
			}
		}
		if ( NULL != command )
		{
			free(command);
			command = NULL;
		}
		if ( NULL != command64 )
		{
			free(command64);
			command64 = NULL;
		}
	}

	/**
	 *	Step 4: Add domain names under your DDNS account if necessary.
	 */
	while ( DDNS_ERROR_SUCCESS == error_code )
	{
		int length = peanuthull_recv(	context,
										peanuthull->buffer,
										sizeof(peanuthull->buffer),
										&error_code
										);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( (-1 == length) || (length >= sizeof(peanuthull->buffer)) )
			{
				error_code = DDNS_ERROR_UNKNOWN;
				break;
			}

			if ( 0 == strcmp(".", peanuthull->buffer) )
			{
				/* hit end */
				break;
			}
			else
			{
				struct ddns_server * domain = NULL;
				domain = (struct ddns_server *)malloc(sizeof(*domain));
				if ( NULL != domain )
				{
					memset(domain, 0, sizeof(*domain));

					c99_strncpy(domain->domain,
								peanuthull->buffer,
								sizeof(domain->domain)
								);
					domain->next = peanuthull->domain_list;
					peanuthull->domain_list = domain;
				}
				else
				{
					error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
					break;
				}
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		peanuthull_keepalive_init(	&(peanuthull->keep_alive.encrypt_ctx),
									challenge,
									challenge_len
									);
	}

	if ( NULL != challenge )
	{
		free(challenge);
		challenge = NULL;
	}

	return error_code;
}


/**
 *	Register a domain for update under your DDNS account after logged in. If the
 *	operation succeeded, the following informations will be kept in the context:
 *
 *		1. User ID.
 *		2. Update sequence number used in keep-alive operations.
 *
 *	@param[in/out]	context	- the DDNS context
 *
 *	@return	int		- positive if successful, or -1 if failed.
 */
static ddns_error peanuthull_register(struct ddns_context *context)
{
	ddns_error						error_code		= DDNS_ERROR_SUCCESS;
	struct peanuthull_context	*	peanuthull		= NULL;

	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL == peanuthull )
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		char					cmd[256];
		struct ddns_server	*	domain	= context->domain;

		if ( NULL == domain )
		{
			domain = peanuthull->domain_list;
		}
		for ( ; NULL != domain; domain = domain->next )
		{
			int length = c99_snprintf(	cmd,
										_countof(cmd),
										PEANUTHULL_CMD_REGISTER,
										domain->domain
										);
			if ( length < _countof(cmd) )
			{
				error_code = peanuthull_sendcmd(context, cmd, 1);
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				if ( 0 != memcmp("250", peanuthull->buffer, 3) )
				{
					error_code = DDNS_ERROR_NOHOST;
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = peanuthull_sendcmd(context, PEANUTHULL_CMD_CONFIRM, 1);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			if ( 0 == memcmp("250", peanuthull->buffer, 3) )
			{
				char* serial_no_pos = strchr(&(peanuthull->buffer[4]), ' ');
				if ( NULL == serial_no_pos )
				{
					error_code = DDNS_ERROR_BADSVR;
				}
				else
				{
					struct peanuthull_keepalive_request * request
						= &(peanuthull->keep_alive.request);
					request->sequence	= atol(serial_no_pos + 1);
					request->session_id	= atol(&(peanuthull->buffer[4]));
				}
			}
			else
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
	}

#if 0
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = peanuthull_sendcmd(context, PEANUTHULL_CMD_USERINFO, 1);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			/* a server message of "250 Userinfo OK" is expected */
			if ( 0 != memcmp("250", peanuthull->buffer, 3) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
		while ( DDNS_ERROR_SUCCESS == error_code )
		{
			int length = peanuthull_recv(	context,
											peanuthull->buffer,
											sizeof(peanuthull->buffer),
											&error_code
											);
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				if ( (-1 == length) || (length >= sizeof(peanuthull->buffer)) )
				{
					error_code = DDNS_ERROR_UNKNOWN;
					break;
				}

				if ( 0 == strcmp(".", peanuthull->buffer) )
				{
					/* hit end */
					break;
				}
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = peanuthull_sendcmd(context, PEANUTHULL_CMD_DOMAININFO, 1);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			/* a server message of "250 DomainInfo OK" is expected */
			if ( 0 != memcmp("250", peanuthull->buffer, 3) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
		while ( DDNS_ERROR_SUCCESS == error_code )
		{
			int length = peanuthull_recv(	context,
											peanuthull->buffer,
											sizeof(peanuthull->buffer),
											&error_code
											);
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				if ( (-1 == length) || (length >= sizeof(peanuthull->buffer)) )
				{
					error_code = DDNS_ERROR_UNKNOWN;
					break;
				}

				if ( 0 == strcmp(".", peanuthull->buffer) )
				{
					/* hit end */
					break;
				}
			}
		}
	}
#endif

	return error_code;
}


/**
 *	Send keep-alive request to active DDNS server via UDP and update sequence
 *	number according to the server response.
 *
 *	@param[in/out]	context			: DDNS context.
 *	@param[in]		request_type	: keep-alive request request_type
 *
 *	@return		Return 0 if successful, otherwise -1 is returned.
 */
static ddns_error peanuthull_do_keepalive(
	struct ddns_context	*	context,
	unsigned long			request_type
	)
{
	ddns_error							error_code	= DDNS_ERROR_SUCCESS;
	struct peanuthull_context		*	peanuthull	= NULL;
	struct peanuthull_keepalive_ctx	*	keep_alive	= NULL;
	struct peanuthull_keepalive_request	request;

	/**
	 *	Step 1: arguments validity check.
	 */
	if ( (NULL == context) || (proto_peanuthull != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		peanuthull = (struct peanuthull_context*)context->extra_data;
		if ( NULL != peanuthull )
		{
			keep_alive = &(peanuthull->keep_alive);
		}
		else
		{
			error_code = DDNS_ERROR_BADARG;
		}
	}

	if (	PEANUTHULL_KEEPALIVE_REQ != request_type
		&&	PEANUTHULL_LOGOUT_REQ != request_type )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		/* compose keep-alive request */

		memcpy(&request, &(keep_alive->request), sizeof(request));
		request.data_type	= request_type;
		request.checksum	= 0 - request.data_type - request.sequence;
		request.reserved	= 0;

		/* encrypt raw request data package */
		Blowfish_Encrypt(	&(keep_alive->encrypt_ctx),
							&(request.data_type),
							&(request.sequence)
							);
		Blowfish_Encrypt(	&(keep_alive->encrypt_ctx),
							&(request.checksum),
							&(request.reserved)
							);

		/* byte-order conversion if needed */
		request.session_id	= ddns_DN2L(request.session_id);
		request.data_type	= ddns_DN2L(request.data_type);
		request.sequence	= ddns_DN2L(request.sequence);
		request.checksum	= ddns_DN2L(request.checksum);
		request.reserved	= ddns_DN2L(request.reserved);
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int length = sendto(peanuthull->sock,
							(char *)(&request),
							sizeof(request),
							0,
							(struct sockaddr*)&context->active_server,
							sizeof(context->active_server)
							);
		if ( -1 == length )
		{
			error_code = DDNS_ERROR_CONNECTION;
		}
	}

	/* step 5: wait for response from DDNS server */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		struct peanuthull_keepalive_reponse svrpkg;
		int buffer_len = peanuthull_recv(	context,
											(char*)&svrpkg,
											sizeof(svrpkg),
											&error_code
											);
		if ( sizeof(svrpkg) == buffer_len )
		{
			/* byte-order conversion if needed */
			svrpkg.session_id	= ddns_DL2N(svrpkg.session_id);
			svrpkg.data_type	= ddns_DL2N(svrpkg.data_type);
			svrpkg.sequence		= ddns_DL2N(svrpkg.sequence);
			svrpkg.checksum		= ddns_DL2N(svrpkg.checksum);
			svrpkg.reserved1	= ddns_DL2N(svrpkg.reserved1);
			svrpkg.address		= ddns_DB2N(svrpkg.address);

			/* decrypt response from DDNS server */
			Blowfish_Decrypt(	&(keep_alive->encrypt_ctx),
								&(svrpkg.data_type),
								&(svrpkg.sequence)
								);
			Blowfish_Decrypt(	&(keep_alive->encrypt_ctx),
								&(svrpkg.checksum),
								&(svrpkg.reserved1)
								);

			if ( svrpkg.session_id == keep_alive->request.session_id )
			{
				switch( request_type )
				{
				case PEANUTHULL_KEEPALIVE_REQ:
					/* update sequence number */
					if ( PEANUTHULL_KEEPALIVE_POS == svrpkg.data_type )
					{
						peanuthull->address				= svrpkg.address;
						keep_alive->request.sequence	= svrpkg.sequence + 1;
					}
					else
					{
						error_code = DDNS_ERROR_UNKNOWN;
					}
					break;

				case PEANUTHULL_LOGOUT_REQ:
					if ( PEANUTHULL_LOGOUT_POS != svrpkg.data_type )
					{
						error_code = DDNS_ERROR_UNKNOWN;
					}
					break;

				default:
					error_code = DDNS_ERROR_UNKNOWN;
					break;
				}
			}
			else
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	return error_code;
}
