/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: dnspod.c 305 2014-06-11 07:32:04Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2009-05-23.
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
 *
 *	Thanks to:
 *		API "/Record.Ddns": Never (986319491@qq.com)
 */
/*
 *	Interface for accessing DNSPod service.
 */

#include "dnspod.h"
#include <stdio.h>		/* sscanf, qsort  */
#include <stdlib.h>		/* malloc		  */
#include <string.h>		/* memset, strlen */
#include <assert.h>		/* assert		  */
#include "json.h"
#include "http.h"
#include "ddns_string.h"


/*============================================================================*
 *	Local Macros & Constants
 *============================================================================*/
#ifndef DNSPOD_USE_DNSPOD
#	define DNSPOD_USE_DNSPOD	1
#endif
#ifndef DNSPOD_USE_BAIDU
#	define DNSPOD_USE_BAIDU		2
#endif
#ifndef DNSPOD_USE_IP138
#	define DNSPOD_USE_IP138		3
#endif

/* Maximum length of URL */
#define DNSPOD_MAX_URL_LENGTH	1024

/* User-Agent to be used in contacting with server. */
#define DNSPOD_USER_AGENT		"ddns-r%s (github.com/NeverMin/ddns/issues)"

/**
 *	Check here for a detail explanation of how to use these commands:
 *
 *	http://support.dnspod.com/index.php?_m=knowledgebase&_a=viewarticle&kbarticleid=43&nav=0
 */
static const char DNSPOD_API_VERSION[]		= "/Info.Version";
static const char DNSPOD_DOMAIN_CREATE[]	= "/Domain.Create";
static const char DNSPOD_DOMAIN_LIST[]		= "/Domain.List";
static const char DNSPOD_DOMAIN_DELETE[]	= "/Domain.Remove";
static const char DNSPOD_DOMAIN_STATUS[]	= "/Domain.Status";
static const char DNSPOD_DOMAIN_PRIV[]		= "/Domain.Purview";
static const char DNSPOD_RECORD_CREATE[]	= "/Record.Create";
static const char DNSPOD_RECORD_LIST[]		= "/Record.List";
static const char DNSPOD_RECORD_MODIFY[]	= "/Record.Modify";
static const char DNSPOD_RECORD_DELETE[]	= "/Record.Remove";
static const char DNSPOD_RECORD_STATUS[]	= "/Record.Status";
static const char DNSPOD_RECORD_DDNS[]		= "/Record.Ddns";

static const char DNSPOD_CMD_API_VERSION[]		= "format=json&login_email=%s&login_password=%s";
static const char DNSPOD_CMD_CREATE_DOMAIN[]	= "format=json&login_email=%s&login_password=%s&domain=%s";
static const char DNSPOD_CMD_CREATE_RECORD[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu&sub_domain=%.*s&record_type=%s&record_line=%s&value=%s&mx=%u&ttl=%u";
static const char DNSPOD_CMD_LIST_DOMAIN[]		= "format=json&login_email=%s&login_password=%s";
static const char DNSPOD_CMD_LIST_DOMAIN_V2[]	= "format=json&login_email=%s&login_password=%s&type=mine&offset=0&length=10000";
static const char DNSPOD_CMD_DOMAIN_PRIV[]		= "format=json&login_email=%s&login_password=%s&domain_id=%lu";
static const char DNSPOD_CMD_LIST_RECORD[]		= "format=json&login_email=%s&login_password=%s&domain_id=%lu";
static const char DNSPOD_CMD_LIST_RECORD_V2[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu&offset=0&length=10000";
static const char DNSPOD_CMD_REMOVE_DOMAIN[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu";
static const char DNSPOD_CMD_REMOVE_RECORD[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu&record_id=%lu";
static const char DNSPOD_CMD_STATUS_DOMAIN[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu&status=%s";
static const char DNSPOD_CMD_STATUS_RECORD[]	= "format=json&login_email=%s&login_password=%s&domain_id=%lu&record_id=%lu&status=%s";
static const char DNSPOD_CMD_SET_RECORD[]		= "format=json&login_email=%s&login_password=%s&domain_id=%lu&record_id=%lu&sub_domain=%s&record_type=%s&record_line=%s&value=%s&mx=%u&ttl=%u";
static const char DNSPOD_CMD_UPDATE_DDNS[]		= "format=json&login_email=%s&login_password=%s&domain_id=%lu&record_id=%lu&sub_domain=%s&record_line=%s&value=%s";

/**
 *	Maximum allowed record TTL, change it carefully. DNS records will cached by
 *	local servers according to this TTL. If it's too large, it'll take you more
 *	time to make the updated record take effect. If it's too small, it will
 *	consume more resource of your main DNS server.
 */
const int	DNSPOD_MIN_TTL		= 60;
const int	DNSPOD_MAX_TTL		= 3600;
const int	DNSPOD_DDNS_TTL		= 10;

/**
 *	All supported DNSPod API versions.
 */
#define DNSPOD_API_VERSION_1_1		0x10001
#define DNSPOD_API_VERSION_2_0		0x20000
#define DNSPOD_API_VERSION_2_1		0x20001
#define DNSPOD_API_VERSION_2_2		0x20002
#define DNSPOD_API_VERSION_2_5		0x20005
#define DNSPOD_API_VERSION_2_8		0x20008
#define DNSPOD_API_VERSION_2_9		0x20009
#define DNSPOD_API_VERSION_3_5		0x30005
#define DNSPOD_API_VERSION_3_6		0x30006
#define DNSPOD_API_VERSION_4_0		0x40000
#define DNSPOD_API_VERSION_4_2		0x40002
#define DNSPOD_API_VERSION_4_3		0x40003
#define DNSPOD_API_VERSION_4_4		0x40004
#define DNSPOD_API_VERSION_4_5		0x40005
#define DNSPOD_API_VERSION_4_6		0x40006

/**
 *	Highest DNSPod API version that we supported.
 */
const ddns_ulong32	DNSPOD_MAX_API_VERSION	= DNSPOD_API_VERSION_4_6;


/*============================================================================*
 *	Declaration of Local Types & Functions
 *============================================================================*/
struct dnspod_context;
struct dnspod_buffer;
struct dnspod_line_table_record;
struct dnspod_getip_table_entry;

/**
 * Prototype of functions used to get IP address.
 */
typedef ddns_error (*DNSPOD_GETIP)(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	);

/**
 *	Structure to keep DNSPod specific information in DDNS context.
 */
struct dnspod_context
{
	char							ip_address[16];
	ddns_ulong32					api_version;
	struct dnspod_domain		*	domain_list;
};

/**
 *	Represent as a DNS domain.
 */
struct dnspod_domain
{
	unsigned long					domain_id;
	char							domain[256];
	int								min_ttl;		/* minimum allowed TTL */
	struct dnspod_record		*	records;
	struct dnspod_domain		*	next;
};

/**
 *	Represent as a DNS entry.
 */
struct dnspod_record
{
	unsigned long					host_id;
	char							name[128];
	enum dnspod_record_line			line;
	enum dnspod_record_type			type;
	unsigned long					ttl;
	char							value[512];
	unsigned long					mx;
	int								enabled;
	char							last_update[32];
	struct dnspod_record		*	next;
};

/**
 *	HTTP response buffer.
 */
struct dnspod_buffer
{
	char			*	buffer;
	unsigned int		used;
	unsigned int		size;
};

/**
 *	Network conversion table.
 */
struct dnspod_line_table_record
{
	ddns_ulong32				api_version;	/* minimum api version		 */
	const char				*	name;			/* line name, utf-8 encoding */
	enum dnspod_record_line		line;			/* line type				 */
};

/**
 * Table entry of functions to get IP address
 */
struct dnspod_getip_table_entry
{
	const char					*	name;
	const char					*	url;
	ddns_long32						priority;
	DNSPOD_GETIP					func;
};

/**
 *	Initialize DDNS context for DNSPod service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error dnspod_interface_initialize(struct ddns_context * context);


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
static ddns_error dnspod_interface_is_ip_changed(struct ddns_context * context);


/**
 *	Get current ip address.
 *
 *	@param[out] buffer	: buffer to save the ip address.
 *	@param[in]	length	: size of the buffer in characters.
 *
 *	NOTE:
 *		This function is optional, return [DDNS_ERROR_NOTIML] if it's not
 *		implemented.
 */
static ddns_error dnspod_interface_get_ip_address(
	struct ddns_context *	context,
	char				*	buffer,
	size_t					length
	);


/**
 *	Update all records via DNSPod service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dnspod_interface_do_update(struct ddns_context * context);


/**
 *	Finalize DDNS context for DNSPod service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dnspod_interface_finalize(struct ddns_context * context);


/**
 *	Destroy DDNS interface object created by [dnspod_create_interface].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error dnspod_interface_destroy(ddns_interface* interface);


/**
 *	Update address of a all requested DNS record.
 *
 *	@param[in]		context		: the DDNS context.
 *	@param[in/out]	domain_list : domain list under your DNSPod account.
 *	@param[in]		address		: the new address for the DNS host.
 *	@param[out]		update_cnt	: number of records that are updated.
 *
 *	@note		It will try to get DNS record information from server first, and
 *				keep it in [domain_list] if successful. It will then compare the
 *				requested address with the address on server. If the 2 addresses
 *				are the same, it'll ignore the request and return 0.
 *
 *	@note		It's safe to pass [NULL] to [update_cnt].
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if update of all DNS records are
 *				successful, otherwise return an error code.
 */
static ddns_error dnspod_interface_update_all(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain_list,
	const char				*	address,
	unsigned int			*	update_cnt
	);


/**
 *	Get all A records in the domains and fill them to DDNS context.
 *
 *	@param[in/out]	context		: the DDNS context.
 *	@param[in]		domain_list : list of domains to be retrieved.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if every thing's going fine.
 *				Otherwise a error code will be returned.
 */
static ddns_error dnspod_interface_list_record(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain_list
	);


/**
 *	Get API version of the DNSPod server.
 *
 *	@param[in/out]	context		: the DDNS context.
 *
 *	@return		Higher 2 bytes represents the major version, lower 2 bytes
 *				represents as the minor version. If any error occurred, 0 will
 *				be returned.
 */
static ddns_error dnspod_get_api_version(
	struct ddns_context		*	context,
	ddns_ulong32			*	api_version
	);

/**
 *	Retrieve additional information of a domain that's not provided in domain
 *	list by default.
 *
 *	@param[in/out]	context		: the DDNS context.
 *	@param[in/out]	domain		: the domain to be handled.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if every thing's going fine.
 *				Otherwise a error code will be returned.
 *
 */
static ddns_error dnspod_get_domain_priv(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain
	);

/**
 *	Send a command to DNSPod server.
 *
 *	@param[in]	context :	the DDNS context
 *	@param[in]	url		:	the target URL to send the command
 *	@param[in]	cmd		:	the command to be sent to server
 *	@param[out] json	:	the [json_value] object constructed from the server
 *							response. Use [json_destroy] to free the returned
 *							object when it's no longer needed.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if the command is successfully
 *				sent to server and get response from server. Otherwise a error
 *				code will be returned.
 */
static ddns_error dnspod_send_command(
	const struct ddns_context	*	context,
	const char					*	url,
	const char					*	cmd,
	struct json_value			**	json
	);


/**
 *	HTTP callback function, it will be called for each received byte and
 *	transfer to byte to json parser.
 */
static void dnspod_http2json(
	char						chr,
	struct json_context		*	context
	);


/**
 *	HTTP callback function, it will be called for each received byte and
 *	keep the received bytes.
 */
static void dnspod_http2buffer(
	char						chr,
	struct dnspod_buffer	*	buffer
	);


/**
 *	Get domain information from the domain name list.
 *
 *	@param[in]	domain_list :	domain name list
 *	@param[in]	domain_name :	the domain name to be searched
 *
 *	@note		Ex.: [domain_name] = "www.website.com", it will search for
 *				domain "website.com"
 *
 *	@return		Find domain and return information of the domain in the domain
 *				list. If it's not found, NULL will be returned.
 */
static const struct dnspod_domain * dnspod_find_domain(
	const struct dnspod_domain	*	domain_list,
	const char					*	domain_name
	);


/**
 *	Get DNS record information from the DNS record list.
 *
 *	@param[in]	host_list	:	domain name list
 *	@param[in]	record_type :	type of the record searching for.
 *	@param[in]	domain_name :	the domain name to be searched for.
 *
 *	@note		Ex.: if [domain_name] = "www.website.com", if will search for
 *				host "www".
 *
 *	@return		Find DNS host and return information of the host in the host
 *				list. If it's not found, NULL will be returned.
 */
static const struct dnspod_record * dnspod_find_record(
	const struct dnspod_record	*	host_list,
	enum dnspod_record_type			record_type,
	const char					*	domain_name
	);


/**
 *	Get internet IP address of local computer.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out] text_buffer : buffer to save the IP address.
 *	@param[in[	buffer_size : size of the buffer.
 *
 *	@note		The returned IP address is like "123.123.123.123".
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successfully get the internet
 *				IP address, otherwise return an error code.
 */
static ddns_error dnspod_get_ip_address(
	struct ddns_context *	context,
	char				*	text_buffer,
	int						buffer_size
	);

/**
 *	Parse server response when getting IP address from server.
 *
 *	@param[in/out]	server_buffer	: server response
 *	@param[out]		text_buffer		: parsed ip address
 *	@param[in]		buffer_size		: size of text_buffer in bytes
 *
 *	@return The following value may be returned:
 *				- DDNS_ERROR_SUCCESS:	legal server response is detected
 *				- DDNS_ERROR_REDIRECT:	redirection is required, redirect URL
 *										is saved in server_buffer.
 *				- DDNS_ERROR_BADSVR:    fail, shall not retry any more.
 *				- Others:				error occurred.
 */
static ddns_error dnspod_get_ip_address_dnspod(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	);
static ddns_error dnspod_get_ip_address_baidu(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	);
static ddns_error dnspod_get_ip_address_ip138(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	);


/**
 *	Compare priority of 2 servers for getting IP address.
 *
 *	@param[in]	entry1	: the first server record
 *	@param[in]	entry2	: the second server record
 *
 *	@return Return the compare result of the 2 records:
 *				entry1 < entry2: negative
 *				entry1 = entry2: 0
 *				entry1 > entry2: positive
 */
static int dnspod_compare_entry(
	const struct dnspod_getip_table_entry * entry1,
	const struct dnspod_getip_table_entry * entry2
	);

/**
 *	Add information of a domain into a domain list.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_info : the domain to be added to the domain list.
 *	@param[out] domain_list : pointer to the domain list.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] on success, otherwise an error
 *				code will be returned.
 */
static ddns_error dnspod_domain_list_push(
	struct ddns_context		*	context,
	struct json_value		*	domain_info,
	struct dnspod_domain	**	domain_list
	);


/**
 *	Get human readable name of a record type.
 *
 *	@param[in]	type		: the DNS record type.
 *
 *	@return		Return pointer to the statically allocated type name.
 */
static const char * dnspod_record_type_name(enum dnspod_record_type type);


/**
 *	Convert human readable name of record type to [dnspod_record_type].
 *
 *	@param[in]	type		: DNS record type
 *
 *	@return		Type of the DNS record.
 */
static enum dnspod_record_type dnspod_get_record_type(const char * type);


/**
 *	Get human readable name of a record line.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	type		: the DNS record line.
 *
 *	@return		Return pointer to the statically allocated line name (utf-8).
 */
static const char * dnspod_record_line_name(
	const struct ddns_context	*	context,
	enum dnspod_record_line			line
	);


/**
 *	Convert human readable line type to type [dnspod_record_line].
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	line		: line type in utf-8 encoding.
 *
 *	@return		Type of the network which your DNS record associated with.
 */
static enum dnspod_record_line dnspod_get_record_line(
	const struct ddns_context	*	context,
	const char					*	line
	);

/**
 *	Convert common server error code into ours.
 *
 *	@param[in]	json		: the json object returned from server.
 *	@param[out]	error_code	: the converted error code.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if it's recognized as succesful
 *				or any known error, in such case, [error_code] will be set to
 *				the converted error code. Otherwise [DDNS_ERROR_UNKNOWN] will be
 *				returned and [error_code] will be set to the raw error code
 *				returned from server.
 */
static ddns_error dnspod_handle_common_error(
	struct json_value			*	json,
	ddns_error					*	error_code
	);

/*============================================================================*
 *	Implementation of mapping tables
 *============================================================================*/
static struct dnspod_line_table_record dnspod_record_line_table[] =
{
	{ DNSPOD_API_VERSION_1_1,	"default",					DNSPOD_LINE_ALL },
	{ DNSPOD_API_VERSION_1_1,	"tel",						DNSPOD_LINE_CTC },
	{ DNSPOD_API_VERSION_1_1,	"cnc",						DNSPOD_LINE_CNC },
	{ DNSPOD_API_VERSION_1_1,	"edu",						DNSPOD_LINE_EDU },
	{ DNSPOD_API_VERSION_1_1,	"cmc",						DNSPOD_LINE_CMC },
	{ DNSPOD_API_VERSION_1_1,	"foreign",					DNSPOD_LINE_FRGN},
	{ DNSPOD_API_VERSION_2_0,	"\xe9\xbb\x98\xe8\xae\xa4", DNSPOD_LINE_ALL },
	{ DNSPOD_API_VERSION_2_0,	"\xe7\x94\xb5\xe4\xbf\xa1", DNSPOD_LINE_CTC },
	{ DNSPOD_API_VERSION_2_0,	"\xe7\xbd\x91\xe9\x80\x9a", DNSPOD_LINE_CNC },
	{ DNSPOD_API_VERSION_2_0,	"\xe6\x95\x99\xe8\x82\xb2\xe7\xbd\x91", DNSPOD_LINE_EDU },
	{ DNSPOD_API_VERSION_2_0,	"\xe7\xa7\xbb\xe5\x8a\xa8", DNSPOD_LINE_CMC },
	{ DNSPOD_API_VERSION_2_0,	"\xe9\x93\x81\xe9\x80\x9a", DNSPOD_LINE_CRC },
	{ DNSPOD_API_VERSION_2_0,	"\xe8\x81\x94\xe9\x80\x9a", DNSPOD_LINE_UNC },
	{ DNSPOD_API_VERSION_2_0,	"\xe5\x9b\xbd\xe5\xa4\x96", DNSPOD_LINE_FRGN}
};


/*============================================================================*
 *	Implementation of Functions
 *============================================================================*/

/**
 *	Create DDNS interface for accessing DNSPod service.
 *
 *	@return		Return pointer to the newly created interface object, or NULL
 *				if there's an error creating interface object.
 */
ddns_interface * dnspod_create_interface(void)
{
	ddns_interface * ddns = NULL;

	ddns = (ddns_interface*)malloc(sizeof(ddns_interface));
	if ( NULL != ddns )
	{
		memset(ddns, 0, sizeof(*ddns));

		ddns->initialize		= &dnspod_interface_initialize;
		ddns->is_ip_changed		= &dnspod_interface_is_ip_changed;
		ddns->get_ip_address	= &dnspod_interface_get_ip_address;
		ddns->do_update			= &dnspod_interface_do_update;
		ddns->finalize			= &dnspod_interface_finalize;
		ddns->destroy			= &dnspod_interface_destroy;
	}

	return ddns;
}


/**
 *	Initialize DDNS context for DNSPod service.
 *
 *	@param[in/out]	context:	the DDNS context to be initialized.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the DDNS context is successfully
 *				initialized, otherwise an error code will be returned.
 */
static ddns_error dnspod_interface_initialize(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

#if !defined(HTTP_SUPPORT_SSL) || 0 == HTTP_SUPPORT_SSL
	return DDNS_FATAL_ERROR(DDNS_ERROR_SSL_REQUIRED);
#endif

	/**
	 *	Step 1: Allocate memory in DDNS context.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 != ddns_create_extra_param(context, sizeof(*dnspod)) )
		{
			dnspod = (struct dnspod_context*)context->extra_data;
		}
		else
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}
	}

	/**
	 *	Step 2: Get internet address of local computer.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_printf_v(context, msg_type_info, "Get internet IP address... ");
		error_code = dnspod_get_ip_address( context,
											dnspod->ip_address,
											_countof(dnspod->ip_address)
											);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			ddns_printf_v(context, msg_type_info, "%s\n", dnspod->ip_address);
		}
		else
		{
			ddns_printf_v(context, msg_type_info, "failed.\n");
		}
	}

	/**
	 *	Step 3: Check if the DNSPod API is supported by us.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_printf_v(context, msg_type_info, "Checking DNSPod API version... ");
		error_code = dnspod_get_api_version(context, &(dnspod->api_version));
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			ddns_printf_v(context,	msg_type_info,
									"%d.%d\n",
									(int)(dnspod->api_version >> 16),
									(int)(dnspod->api_version & 0xffff)
									);
			if ((dnspod->api_version >> 16) > (DNSPOD_MAX_API_VERSION >> 16))
			{
				error_code = DDNS_ERROR_NOTIMPL;
			}
			else if ( dnspod->api_version > DNSPOD_MAX_API_VERSION )
			{
				ddns_msg(context,	msg_type_warning,
									"API version is higher than expected (%d.%d).\n",
									(int)(DNSPOD_MAX_API_VERSION >> 16),
									(int)(DNSPOD_MAX_API_VERSION & 0xffff)
									);
			}
		}
		else
		{
			ddns_printf_v(context, msg_type_info, "NG\n");
		}
	}

	/**
	 *	Step 4: Get domain list.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_printf_v(context, msg_type_info, "Retrieving domain list... ");
		dnspod->domain_list = dnspod_list_domain(context, &error_code);
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			struct dnspod_domain * domain = dnspod->domain_list;

			ddns_printf_v(context, msg_type_info, "\n");
			if ( NULL == domain )
			{
				ddns_printf_v(context,	msg_type_info, "  * < no domain >\n");
			}
			for ( ; NULL != domain; domain = domain->next )
			{
				ddns_printf_v(context,	msg_type_info,
										"  * %d: %s (Minimum TTL = %d)\n",
										domain->domain_id,
										domain->domain,
										domain->min_ttl);
			}
		}
		else
		{
			ddns_printf_v(context, msg_type_info, "failed.\n");
		}
	}

	/**
	 *	Step 5: get all hosts under your DDNS account if necessary.
	 */
	if ( (DDNS_ERROR_SUCCESS == error_code) )
	{
		if ( NULL == context->domain )
		{
			error_code = dnspod_interface_list_record(	context,
														dnspod->domain_list
														);
		}
	}

	/**
	 *	Step 6: Do initial update, make sure we're synchronized with server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_interface_update_all(	context,
													dnspod->domain_list,
													dnspod->ip_address,
													NULL
													);
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
static ddns_error dnspod_interface_is_ip_changed(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;
	char						ip_address[_countof(dnspod->ip_address)];

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_get_ip_address( context,
											ip_address,
											_countof(ip_address)
											);
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 == strcmp(ip_address, dnspod->ip_address) )
		{
			ddns_printf_v(context, msg_type_info, "IP address does not change.\n");
			error_code = DDNS_ERROR_NOCHG;
		}
		else
		{
			ddns_printf_v(context,	msg_type_info,
									"IP address changed to \"%s\".\n",
									ip_address
									);
			c99_strncpy(dnspod->ip_address,
						ip_address,
						_countof(dnspod->ip_address)
						);
		}
	}

	return error_code;
}


/**
 *	Get current ip address.
 *
 *	@param[out] buffer	: buffer to save the ip address.
 *	@param[in]	length	: size of the buffer in characters.
 *
 *	NOTE:
 *		This function is optional, return [DDNS_ERROR_NOTIML] if it's not
 *		implemented.
 */
static ddns_error dnspod_interface_get_ip_address(
	struct ddns_context *	context,
	char				*	buffer,
	size_t					length
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t actual_len = 0;

		actual_len = c99_snprintf(buffer, length, "%s", dnspod->ip_address);
		if ( actual_len >= length )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	return error_code;
}


/**
 *	Update all records via DNSPod service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dnspod_interface_do_update(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_interface_update_all(	context,
													dnspod->domain_list,
													dnspod->ip_address,
													NULL
													);
	}

	return error_code;
}


/**
 *	Finalize DDNS context for DNSPod service.
 *
 *	@param[in]	context		: the DDNS context to operate.
 *
 *	@return		Return DDNS_ERROR_SUCCESS on success, otherwise an error code
 *				will be returned.
 */
static ddns_error dnspod_interface_finalize(struct ddns_context * context)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if( DDNS_ERROR_SUCCESS == error_code )
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( NULL != dnspod->domain_list )
		{
			dnspod_destroy_domain_list(dnspod->domain_list);
			dnspod->domain_list = NULL;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_remove_extra_param(context);
	}

	return error_code;
}


/**
 *	Destroy DDNS interface object created by [dnspod_create_interface].
 *
 *	@param[in]	interface	: the object to be destroyed.
 *
 *	@return		Return DDNS_ERROR_SUCCESS if the object is successfully
 *				destroyed. Otherwise, an error code will be returned.
 */
static ddns_error dnspod_interface_destroy(ddns_interface* ddns)
{
	ddns_error error_code = DDNS_ERROR_SUCCESS;

	if ( NULL != ddns )
	{
		if ( ddns->destroy == &dnspod_interface_destroy )
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
 *	Update address of a all requested DNS record.
 *
 *	@param[in]		context		: the DDNS context.
 *	@param[in/out]	domain_list : domain list under your DNSPod account.
 *	@param[in]		address		: the new address for the DNS host.
 *	@param[out]		update_cnt	: number of records that are updated.
 *
 *	@note		It will try to get DNS record information from server first, and
 *				keep it in [domain_list] if successful. It will then compare the
 *				requested address with the address on server. If the 2 addresses
 *				are the same, it'll ignore the request and return 0.
 *
 *	@note		It's safe to pass [NULL] to [update_cnt].
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if update of all DNS records are
 *				successful, otherwise return an error code.
 */
static ddns_error dnspod_interface_update_all(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain_list,
	const char				*	address,
	unsigned int			*	update_cnt
	)
{
	ddns_error						error_code	= DDNS_ERROR_SUCCESS;
	const struct ddns_server	*	domain		= NULL;

	if ( (NULL == context) || (NULL == domain_list) || (NULL == address) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( NULL != update_cnt )
		{
			(*update_cnt) = 0;
		}

		for ( domain = context->domain; NULL != domain; domain = domain->next )
		{
			ddns_printf_v(context,	msg_type_info,
									"Updating domain name \"%s\"... ",
									domain->domain
									);
			error_code = dnspod_update_address( context,
												domain_list,
												domain->domain,
												address
												);
			if ( DDNS_ERROR_SUCCESS == error_code)
			{
				ddns_printf_v(context, msg_type_info, "done.\n");
				if ( NULL != update_cnt )
				{
					++(*update_cnt);
				}
			}
			else if ( DDNS_ERROR_NOCHG == error_code )
			{
				ddns_printf_v(context, msg_type_info, "skipped.\n");
				error_code = DDNS_ERROR_SUCCESS;
			}
			else
			{
				ddns_printf_v(context, msg_type_info, "failed.\n");
				break;
			}
		}
	}

	return error_code;
}


/**
 *	Get all A records in the domains and fill them to DDNS context.
 *
 *	@param[in/out]	context		: the DDNS context.
 *	@param[in]		domain_list : list of domains to be retrieved.
 *
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if every thing's going fine.
 *				Otherwise a error code will be returned.
 */
ddns_error dnspod_interface_list_record(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain_list
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct ddns_server			ddns_host;
	struct dnspod_domain	*	domain		= NULL;

	memset(&ddns_host, 0, sizeof(ddns_host));

	for ( domain = domain_list; NULL != domain; domain = domain->next )
	{
		struct dnspod_record *	record	= NULL;

		if ( NULL != domain->records )
		{
			continue;
		}

		ddns_printf_v(context,	msg_type_info,
								"Retrieving DNS records of domain \"%s\"... ",
								domain->domain
								);

		domain->records = dnspod_list_record(context,
											 domain,
											 &error_code
											 );
		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			struct dnspod_record *record = domain->records;

			ddns_printf_v(context, msg_type_info, "\n");
			if ( NULL == record )
			{
				ddns_printf_v(context, msg_type_info, "  * < no record >\n");
			}
			for ( ; NULL != record; record = record->next )
			{
				if ( 0 == record->enabled )
				{
					continue;
				}
				ddns_printf_v(context,	msg_type_info,
										"  * %d: [%s] %s.%s = %s\n",
										record->host_id,
										dnspod_record_type_name(record->type),
										record->name,
										domain->domain,
										record->value
										);
			}
		}
		else
		{
			ddns_printf_v(context, msg_type_info, "failed.\n");
			break;
		}

		for ( record = domain->records; NULL != record; record = record->next )
		{
			size_t length = 0;

			/* Only A record is supported */
			if ( (0 == record->enabled) || (DNSPOD_RECORD_TYPE_A != record->type) )
			{
				continue;
			}

			length = c99_snprintf(	ddns_host.domain,
									_countof(ddns_host.domain),
									"%s.%s",
									record->name,
									domain->domain
									);
			if ( length >= _countof(ddns_host.domain) )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
				break;
			}
			else if ( 0 == ddns_adddomain(context, &ddns_host) )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
				break;
			}
		}

		if ( DDNS_ERROR_SUCCESS != error_code )
		{
			break;
		}
	}

	return error_code;
}


/**
 *	Create a domain under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_name : name of the domain to be created.
 *	@param[out] error_code	: to keep the error code if encountered any error.
 *
 *	@return		Return ID of the newly created domain if successful, otherwise
 *				0 will be returned.
 */
unsigned long dnspod_create_domain(
	const struct ddns_context	*	context,
	const char					*	domain_name,
	ddns_error					*	error_code
	)
{
	struct json_value	*	json		= NULL;
	ddns_error				status_code = DDNS_ERROR_SUCCESS;
	unsigned long			domain_id	= 0;

	/**
	 *	Step 1: arguments validity check.
	 */
	if (	(NULL == context) || (proto_dnspod != context->protocol)
		||	(NULL == domain_name) )
	{
		status_code = DDNS_ERROR_BADARG;
	}

	/**
	 *	Step 2: send command to DDNS server
	 */
	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		size_t	length			= 0;
		char	command[1024]	= { 0 };
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_CREATE_DOMAIN,
										username,
										password,
										domain_name
										);
		if ( length >= _countof(command) )
		{
			status_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			status_code = dnspod_send_command(	context,
												DNSPOD_DOMAIN_CREATE,
												command,
												&json
												);
			if (	(DDNS_ERROR_SUCCESS == status_code)
				&&	(json_type_object != json_get_type(json)) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	/**
	 *	Step 3: Analyze server response
	 */
	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		struct json_value	*	id		= NULL;
		struct json_value	*	result	= json_object_get(json, "domain");

		ddns_error err = dnspod_handle_common_error(json, &status_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch (status_code)
			{
			case 6:		/* invalid domain name */
				status_code = DDNS_ERROR_BADDOMAIN;
				break;
			case 7:		/* domain already exists */
				status_code = DDNS_ERROR_DUPDOMAIN;
				break;
			case 11:	/* it's alias of another domain */
				status_code = DDNS_ERROR_DUPDOMAIN;
				break;
			case 12:	/* access denied */
				status_code = DDNS_ERROR_DUPDOMAIN;
				break;
			case 41:	/* illegal website, block it */
				status_code = DDNS_ERROR_BLOCKED;
				break;
			default:
				status_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			status_code = err;
			break;
		}

		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			id = json_object_get(result, "id");
			if ( 0 == json_to_number(id) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}

		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			domain_id = (int)json_number_get(id);
		}

		json_destroy(id);		id		= NULL;
		json_destroy(result);	result	= NULL;
	}

	/**
	 *	Step 4: clean up
	 */
	json_destroy(json);
	json = NULL;

	if ( NULL != error_code )
	{
		(*error_code) = status_code;
	}

	return domain_id;
}


/**
 *	List registered domains under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out] error_code	: to keep the error code if encountered any error.
 *
 *	@note		It's safe to pass NULL to [error_code].
 *
 *	@return		Return pointer to the domain list, when the domain list is no
 *				longer needed, use [dnspod_destroy_domain_list] to free it.
 */
struct dnspod_domain * dnspod_list_domain(
	struct ddns_context		*	context,
	ddns_error				*	error_code )
{
	ddns_error					status_code = DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;
	struct json_value		*	response	= NULL;
	struct dnspod_domain	*	domain_list = NULL;

	char command[512];
	char username[sizeof(context->username) * 3];
	char password[sizeof(context->password) * 3];

	memset(&command, 0, sizeof(command));
	memset(&username, 0, sizeof(username));
	memset(&password, 0, sizeof(password));

	if ( (NULL == context) || (proto_dnspod != context->protocol) )
	{
		status_code = DDNS_ERROR_BADARG;
	}
	else
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			status_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		size_t length = 0;

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										(dnspod->api_version >= DNSPOD_API_VERSION_2_0
											? DNSPOD_CMD_LIST_DOMAIN_V2
											: DNSPOD_CMD_LIST_DOMAIN),
										username,
										password
										);
		if ( length >= _countof(command) )
		{
			status_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			status_code = dnspod_send_command(	context,
												DNSPOD_DOMAIN_LIST,
												command,
												&response
												);
			if (	(DDNS_ERROR_SUCCESS == status_code)
				&&	(json_type_object != json_get_type(response)) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	/**
	 *	Step 1: Read status code, it must be "DNSPOD_ERROR_SUCCESS".
	 */
	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		ddns_error err = dnspod_handle_common_error(response, &status_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch (status_code)
			{
			case 6:		/* invalid domain offset */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 7:		/* invalid domain count */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 9:		/* no domain found */
				status_code = DDNS_ERROR_EMPTY;
				break;
			default:
				status_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			status_code = err;
			break;
		}
	}

	/**
	 *	STEP 2: Read domain list.
	 */
	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		struct json_value	*	domain	= NULL;
		struct json_value	*	ary		= NULL;

		domain = json_object_get(response, "domains");
		if (	(dnspod->api_version < DNSPOD_API_VERSION_2_0)
			&&	(json_type_object != json_get_type(domain)) )
		{
			status_code = DDNS_ERROR_BADSVR;
		}

		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			if ( dnspod->api_version >= DNSPOD_API_VERSION_2_0 )
			{
				ary = json_duplicate(domain);
			}
			else
			{
				ary = json_object_get(domain, "domain");
			}

			if ( json_type_array != json_get_type(ary) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}
		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			signed long idx = 0;
			signed long domain_count = (signed long)json_array_size(ary);
			for ( idx = domain_count - 1; idx >= 0; --idx )
			{
				struct json_value * domain_info = json_array_get(ary, idx);

				status_code = dnspod_domain_list_push(	context,
														domain_info,
														&domain_list );

				json_destroy(domain_info);
				domain_info = NULL;

				if ( DDNS_ERROR_SUCCESS != status_code )
				{
					break;
				}
			}
		}

		json_destroy(ary);		ary = NULL;
		json_destroy(domain);	domain	= NULL;
	}

	json_destroy(response);
	response = NULL;

	/* Special case: successful, no no record returned from server */
	if ( DDNS_ERROR_EMPTY == status_code )
	{
		status_code = DDNS_ERROR_SUCCESS;
		ddns_printf_v(context, msg_type_info, "\n  * < no record >\n");
	}

	if ( NULL != error_code )
	{
		(*error_code) = status_code;
	}

	return domain_list;
}


/**
 *	Destroy & free resources allocated for a domain list.
 *
 *	@param	list		: the domain list to be freed.
 */
void dnspod_destroy_domain_list(struct dnspod_domain * list)
{
	while( NULL != list )
	{
		struct dnspod_domain * next = list->next;

		if ( NULL != list->records )
		{
			dnspod_destroy_record_list(list->records);
			list->records = NULL;
		}

		free(list);

		list = next;
	}
}


/**
 *	Remove a domain from your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_name : name of the domain to be removed.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successful, otherwise return
 *				an error code.
 */
ddns_error dnspod_remove_domain_by_name(
	struct ddns_context		*	context,
	const char				*	domain_name
	)
{
	struct json_value			*	json		= NULL;
	struct dnspod_domain		*	domain_list = NULL;
	const struct dnspod_domain	*	domain		= NULL;
	ddns_error						error_code = DDNS_ERROR_SUCCESS;

	ddns_printf_n(context, msg_type_info, "trying to remove domain name \"%s\"... ", domain_name);

	/**
	 *	Step 1: arguments validity check.
	 */
	if (	(NULL == context) || (proto_dnspod != context->protocol)
		||	(NULL == domain_name) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	/**
	 *	Step 2: List all domains under your account.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		domain_list = dnspod_list_domain(context, &error_code);
	}

	/**
	 *	Step 3: Find information of the domain to be removed.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		struct dnspod_domain * lst = domain_list;
		for ( ; NULL != lst; lst = lst->next )
		{
			if ( 0 == ddns_strcasecmp(lst->domain, domain_name) )
			{
				domain = lst;
				break;
			}
		}
		if ( NULL == domain )
		{
			error_code = DDNS_ERROR_NODOMAIN;
		}
	}

	/**
	 *	Step 4: send command to server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t	length			= 0;
		char	command[1024]	= { 0 };
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_REMOVE_DOMAIN,
										username,
										password,
										domain->domain_id
										);
		if ( length >= _countof(command) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			error_code = dnspod_send_command(	context,
												DNSPOD_DOMAIN_DELETE,
												command,
												&json
												);
			if (	(DDNS_ERROR_SUCCESS == error_code)
				&&	(json_type_object != json_get_type(json)) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	/**
	 *	Step 5: Analyze response from DDNS server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_error err = dnspod_handle_common_error(json, &error_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch (error_code)
			{
			case -15:	/* domain is blocked */
				error_code = DDNS_ERROR_BLOCKED;
				break;
			case 6:		/* illegal domain id */
				error_code = DDNS_ERROR_NXDOMAIN;
				break;
			case 7:		/* domain is locked */
				error_code = DDNS_ERROR_BLOCKED;
				break;
			case 8:		/* can't remove VIP domain */
				error_code = DDNS_ERROR_BLOCKED;
				break;
			case 9:		/* not owner of the domain */
				error_code = DDNS_ERROR_NODOMAIN;
				break;
			default:
				error_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			error_code = err;
			break;
		}
	}

	/**
	 *	Step 6: clean up
	 */
	dnspod_destroy_domain_list(domain_list);
	domain_list = NULL;
	json_destroy(json);
	json		= NULL;

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_printf_n(context, msg_type_info, "done.\n");
	}
	else
	{
		ddns_printf_n(context, msg_type_info, "failed.\n");
	}

	return error_code;
}


/**
 *	List DNS record for a domain under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain		: the domain to be listed.
 *	@param[out] error_code	: to keep the error code if encountered any error.
 *
 *	@note		It's safe to pass NULL to [error_code].
 *
 *	@return		Return pointer to the record list, when the record list is no
 *				longer needed, use [dnspod_destroy_record_list] to free it.
 */
struct dnspod_record * dnspod_list_record(
	const struct ddns_context	*	context,
	const struct dnspod_domain	*	domain,
	ddns_error					*	error_code
	)
{
	ddns_error						status_code = DDNS_ERROR_SUCCESS;
	struct json_value			*	json		= NULL;
	struct dnspod_context		*	dnspod		= NULL;
	struct dnspod_record		*	list		= NULL;

	char command[512];
	char username[sizeof(context->username) * 3];
	char password[sizeof(context->password) * 3];

	memset(&command, 0, sizeof(command));
	memset(&username, 0, sizeof(username));
	memset(&password, 0, sizeof(password));

	if ( (NULL == context) || (NULL == domain) )
	{
		status_code = DDNS_ERROR_BADARG;
	}
	else
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			status_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		size_t length = 0;

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										(dnspod->api_version >= DNSPOD_API_VERSION_2_0
											? DNSPOD_CMD_LIST_RECORD_V2
											: DNSPOD_CMD_LIST_RECORD),
										username,
										password,
										domain->domain_id
										);
		if ( length >= _countof(command) )
		{
			status_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			status_code = dnspod_send_command(	context,
												DNSPOD_RECORD_LIST,
												command,
												&json
												);
			if (	(DDNS_ERROR_SUCCESS == status_code)
				&&	(json_type_object != json_get_type(json)) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}
	}

	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		ddns_error err = dnspod_handle_common_error(json, &status_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch (status_code)
			{
			case -7:	/* the enterprise account need upgrade */
			case -8:	/* the domain need upgrade */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case 6:		/* illegal domain id */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_NXDOMAIN);
				break;
			case 7:		/* domain is locked */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case 8:		/* illegal domain count */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 9:		/* not owner of the domain */
				status_code = DDNS_FATAL_ERROR(DDNS_ERROR_NODOMAIN);
				break;
			case 10:	/* no record found */
				status_code = DDNS_ERROR_EMPTY;
				break;
			default:
				status_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			status_code = err;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == status_code )
	{
		struct json_value	*	records		= json_object_get(json, "records");
		struct json_value	*	host_list	= NULL;

		if (	(dnspod->api_version < DNSPOD_API_VERSION_2_0)
			&&	(json_type_object != json_get_type(records)) )
		{
			status_code = DDNS_ERROR_BADSVR;
		}

		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			if ( dnspod->api_version >= DNSPOD_API_VERSION_2_0 )
			{
				host_list = json_duplicate(records);
			}
			else
			{
				host_list = json_object_get(records, "record");
			}
			if ( json_type_array != json_get_type(host_list) )
			{
				status_code = DDNS_ERROR_BADSVR;
			}
		}

		if ( DDNS_ERROR_SUCCESS == status_code )
		{
			signed long idx			= 0;
			signed long host_count	= (signed long)json_array_size(host_list);
			for ( idx = host_count - 1; idx >= 0; --idx )
			{
				struct json_value * host	= json_array_get(host_list, idx);
				struct json_value * id		= NULL;
				struct json_value * name	= NULL;
				struct json_value * line	= NULL;
				struct json_value * type	= NULL;
				struct json_value * ttl		= NULL;
				struct json_value * value	= NULL;
				struct json_value * mx		= NULL;
				struct json_value * enabled = NULL;
				struct json_value * last_update = NULL;
				struct dnspod_record * record	= NULL;
				if ( json_type_object != json_get_type(host) )
				{
					status_code = DDNS_ERROR_BADSVR;
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					id = json_object_get(host, "id");
					if ( 0 == json_to_number(id) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					name = json_object_get(host, "name");
					if ( json_type_string != json_get_type(name) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					line = json_object_get(host, "line");
					if ( json_type_string != json_get_type(line) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					type = json_object_get(host, "type");
					if ( json_type_string != json_get_type(type) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					ttl = json_object_get(host, "ttl");
					if ( json_type_string != json_get_type(ttl) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					value = json_object_get(host, "value");
					if ( json_type_string != json_get_type(value) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					mx = json_object_get(host, "mx");
					if ( json_type_string != json_get_type(mx) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					enabled = json_object_get(host, "enabled");
					if ( json_type_string != json_get_type(enabled) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					last_update = json_object_get(host, "updated_on");
					if ( json_type_string != json_get_type(last_update) )
					{
						status_code = DDNS_ERROR_BADSVR;
					}
				}

				if ( DDNS_ERROR_SUCCESS == status_code )
				{
					record = malloc(sizeof(*record));
					if ( NULL != record )
					{
						memset(record, 0, sizeof(*record));
						record->host_id = (int)json_number_get(id);
						record->line	= dnspod_get_record_line(context, json_string_get(line));
						record->type	= dnspod_get_record_type(json_string_get(type));
						record->ttl		= strtoul(json_string_get(ttl), NULL, 10);
						record->mx		= strtoul(json_string_get(mx), NULL, 10);
						record->enabled = (0 != atol(json_string_get(enabled)));
						c99_strncpy(record->name,
									json_string_get(name),
									_countof(record->name));
						c99_strncpy(record->value,
									json_string_get(value),
									_countof(record->value));
						c99_strncpy(record->last_update,
									json_string_get(last_update),
									_countof(record->last_update));

						record->next = list;
						list = record;
					}
					else
					{
						status_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
					}
				}

				json_destroy(last_update);	last_update = NULL;
				json_destroy(enabled);		enabled		= NULL;
				json_destroy(mx);			mx			= NULL;
				json_destroy(value);		value		= NULL;
				json_destroy(ttl);			ttl			= NULL;
				json_destroy(type);			type		= NULL;
				json_destroy(line);			line		= NULL;
				json_destroy(name);			name		= NULL;
				json_destroy(id);			id			= NULL;
				json_destroy(host);			host		= NULL;

				if ( DDNS_ERROR_SUCCESS != status_code )
				{
					break;
				}
			}
		}

		json_destroy(host_list);	host_list	= NULL;
		json_destroy(records);		records		= NULL;
	}

	json_destroy(json);
	json = NULL;

	/* Special case: successful, no no record returned from server */
	if ( DDNS_ERROR_EMPTY == status_code )
	{
		status_code = DDNS_ERROR_SUCCESS;
	}

	if ( NULL != error_code )
	{
		(*error_code) = status_code;
	}

	return list;
}


/**
 *	Destroy & free resources allocated for a record list.
 *
 *	@param	list		: the record list to be freed.
 */
void dnspod_destroy_record_list(struct dnspod_record * list)
{
	while( NULL != list )
	{
		struct dnspod_record * next = list->next;
		free(list);
		list = next;
	}
}


/**
 *	Update address of a one DNS record.
 *
 *	@param[in]		context		: the DDNS context.
 *	@param[in/out]	domain_list : domain list under your DNSPod account.
 *	@param[in]		domain_name : the DNS record to be updated.
 *	@param[in]		address		: the new address of the DNS record.
 *
 *	@note		It will try to get DNS record information from server first, and
 *				keep it in [domain_list] if successful. It will then compare the
 *				requested address with the address on server. If the 2 addresses
 *				are the same, it'll ignore the request and return 0.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if update succeeded, return
 *				[DNSPOD_ERROR_NOCHANGE] if the [address] is the same as the
 *				previous value. Otherwise an error code will be returned.
 */
ddns_error dnspod_update_address(
	const struct ddns_context	*	context,
	const struct dnspod_domain	*	domain_list,
	const char					*	domain_name,
	const char					*	address
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_domain	*	domain		= NULL;
	struct dnspod_record	*	record		= NULL;

	/**
	 *	Step 1: Get domain ID from the domain list.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		/**
		 *	Oh yeah, you found it, don't you!
		 *
		 *	Actually, [domain_list] is not 'const', so we can assume the each
		 *	domain element in the list is not 'const' too.
		 */
		domain = (struct dnspod_domain*)dnspod_find_domain(domain_list,
														   domain_name
														   );
		if ( NULL == domain )
		{
			error_code = DDNS_ERROR_NODOMAIN;
		}
	}

	/**
	 *	Step 2: Get host list in the domain.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		/* Try to get information from server first. */
		if ( NULL == domain->records )
		{
			domain->records = dnspod_list_record(context,
												 domain,
												 &error_code
												 );
		}
		if ( (DDNS_ERROR_SUCCESS == error_code) && (NULL == domain->records) )
		{
			error_code = DDNS_ERROR_CONNECTION;
		}
	}

	/**
	 *	Step 3: Get host ID in the domain.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		record = (struct dnspod_record*)dnspod_find_record(domain->records,
														   DNSPOD_RECORD_TYPE_A,
														   domain_name);
		if ( NULL == record )
		{
			error_code = DDNS_ERROR_NOHOST;
		}
	}

	/**
	 *	Step 4: Update host information in the domain if necessary.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int		do_update		= 0;
		long	expected_ttl	= context->interval;

		/**
		 *	If the requested address is the same with the one on server, it's
		 *	safe to skip the update operation.
		 */
		if ( 0 != strncmp(address, record->value, sizeof(record->value)) )
		{
			do_update = 1;
		}

		/**
		 *	Prefer to use "Record.Modify" to "Record.Ddns" when ever possible,
		 *	because we will have better control.
		 */
		if ((expected_ttl * 2) < domain->min_ttl)
		{
			// try the "Record.Modify", TTL is fixed (server constraint) to 10s
			if ( record->ttl != DNSPOD_DDNS_TTL )
			{
				do_update	= 1;
				record->ttl = DNSPOD_DDNS_TTL;
			}

			if ( 0 == do_update )
			{
				error_code = DDNS_ERROR_NOCHG;
			}
			else
			{
				c99_strncpy(record->value, address, _countof(record->value));
				error_code = dnspod_update_ddns(context, domain->domain_id, record);
			}
		}
		else
		{
			// Try "Record.Modify".

			/**
			 *	DNS record TTL must not be too great.
			 */
			if ( record->ttl != expected_ttl )
			{
				if ( expected_ttl < domain->min_ttl )
				{
					expected_ttl = domain->min_ttl;
				}
				else if ( expected_ttl > DNSPOD_MAX_TTL )
				{
					expected_ttl = DNSPOD_MAX_TTL;
				}
				if ( record->ttl != expected_ttl )
				{
					do_update	= 1;
					record->ttl = expected_ttl;
				}
			}

			if ( 0 == do_update )
			{
				error_code = DDNS_ERROR_NOCHG;
			}
			else
			{
				c99_strncpy(record->value, address, _countof(record->value));
				error_code = dnspod_update_record(context, domain->domain_id, record);
			}
		}
	}

	return error_code;
}

/**
 *	Update a DNS record.
 *
 *	@param[in]	context		: the DDNS context
 *	@param[in]	domain_id	: domain identifier
 *	@param[in]	record		: DNS record to be set
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successful, otherwise return
 *				an error code.
 */
ddns_error dnspod_update_record(
	const struct ddns_context	*	context,
	unsigned long					domain_id,
	const struct dnspod_record	*	record
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;
	struct json_value		*	value		= NULL;
	size_t						length		= 0;
	char						command[2048];

	memset(command, 0, sizeof(command));

	if ( NULL == context )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_SET_RECORD,
										username, password,
										domain_id, record->host_id,
										record->name,
										dnspod_record_type_name(record->type),
										dnspod_record_line_name(context,
																record->line),
										record->value,
										record->mx,
										record->ttl
										);
		if ( length >= _countof(command) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_send_command(	context,
											DNSPOD_RECORD_MODIFY,
											command,
											&value
											);
		if (	(DDNS_ERROR_SUCCESS == error_code)
			&&	(json_type_object != json_get_type(value)) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_error err = dnspod_handle_common_error(value, &error_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch (error_code)
			{
			case -15:	/* domain is blocked */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case -7:	/* the enterprise account need upgrade */
			case -8:	/* the domain need upgrade */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case 6:		/* illegal domain id */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_NXDOMAIN);
				break;
			case 7:		/* access denied */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_NOHOST);
				break;
			case 8:		/* illegal record id */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 21:	/* domain is locked */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case 22:	/* illegal host name */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADDOMAIN);
				break;
			case 23:	/* too deep sub-domain */
			case 24:	/* wildcard not supported */
			case 25:	/* too many polling records */
			case 26:	/* unsupported network type */
			case 27:	/* unsupported record type */
			case 29:	/* TTL is too small */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case 30:	/* MX exceed allowed range (1-20) */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 31:	/* too many URL records */
			case 32:	/* too many NS records */
			case 33:	/* too many AAAA records */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case 34:	/* illegal record value */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 35:	/* try to set blocked IP address as record value */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			case 36:	/* only 'default' is allowed for NS record of "@" */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADARG);
				break;
			default:
				error_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			error_code = err;
			break;
		}
	}

	json_destroy(value);
	value = NULL;

	return error_code;
}

/**
 *	Update a DNS record (10s fixed TTL).
 *
 *	@param[in]	context		: the DDNS context
 *	@param[in]	domain_id	: domain identifier
 *	@param[in]	record		: DNS record to be set
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successful, otherwise return
 *				an error code.
 */
ddns_error dnspod_update_ddns(
	const struct ddns_context	*	context,
	unsigned long					domain_id,
	const struct dnspod_record	*	record
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct dnspod_context	*	dnspod		= NULL;
	struct json_value		*	json		= NULL;
	char						command[1024];

	memset(command, 0, sizeof(command));

	if ( NULL == context )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		dnspod = (struct dnspod_context*)context->extra_data;
		if ( NULL == dnspod )
		{
			error_code = DDNS_ERROR_UNINIT;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int		length = 0;
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_UPDATE_DDNS,
										username, password,
										domain_id, record->host_id,
										record->name,
										dnspod_record_line_name(context,
																record->line),
										record->value
										);
		if ( length >= _countof(command) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_send_command(	context,
											DNSPOD_RECORD_DDNS,
											command,
											&json
											);
		if (	(DDNS_ERROR_SUCCESS == error_code)
			&&	(json_type_object != json_get_type(json)) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_error err = dnspod_handle_common_error(json, &error_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;
		case DDNS_ERROR_NOTIMPL:
			switch (error_code)
			{
			case -15:	/* domain is blocked */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case -8:	/* the domain under agent needs upgrade */
			case -7:	/* the domain under enterprise account needs upgrade */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case 6:		/* incorrect domain id */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_NXDOMAIN);
				break;
			case 7:		/* access denied to the domain */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_NOHOST);
				break;
			case 8:		/* incorrect host id */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_NXDOMAIN);
				break;
			case 21:	/* domain is locked */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case 22:	/* illegal host name */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADDOMAIN);
				break;
			case 23:	/* too deep sub-domain */
			case 24:	/* wildcard not supported */
			case 25:	/* too many polling records */
			case 26:	/* unsupported network type */
				error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			default:
				error_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;
		default:
			error_code = err;
			break;
		}
	}

	return error_code;
}


/**
 *	Send a command to DNSPod server.
 *
 *	@param[in]	context :	the DDNS context
 *	@param[in]	path	:	the target URL to send the command
 *	@param[in]	cmd		:	the command to be sent to server
 *
 *	@return		Return the [json_value] object constructed from the server
 *				response. Use [json_destroy] to free the returned object when
 *				it's no longer needed.
 */
static ddns_error dnspod_send_command(
	const struct ddns_context	*	context,
	const char					*	path,
	const char					*	cmd,
	struct json_value			**	json_value
	)
{
	ddns_error					error_code	= DDNS_ERROR_SUCCESS;
	struct http_request		*	request		= NULL;
	struct json_context		*	json_ctx	= NULL;

	/**
	 *	Step 1: Arguments validity check.
	 */
	if ( (NULL == context) || (NULL == path) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		json_ctx = json_create_context(20);
		if ( NULL == json_ctx )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}
	}

	/**
	 *	Step 2: Create HTTP request.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t	length = 0;
		char	URL[1024];

		length = c99_snprintf(URL,	_countof(URL), "%s%s:%d%s",
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
									"https://",
#else
									"http://",
#endif
									context->server->domain,
									context->server->port,
									path
									);
		if ( length >= _countof(URL) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			request = http_create_request(	http_method_post,
											URL,
											context->timeout
											);
			if ( NULL == request )
			{
				error_code = DDNS_ERROR_BADURL;
			}
		}
	}

	/**
	 *	Step 3: Connect to server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int result = http_connect(request);
		switch( result )
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

	/**
	 *	Step 4: Add additional HTTP request header.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 == http_add_header(	request,
									"Content-Type",
									"application/x-www-form-urlencoded", 1) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
		}
		else
		{
			size_t	length = 0;
			char user_agent[64];

			length = c99_snprintf(user_agent,	_countof(user_agent),
												DNSPOD_USER_AGENT,
												ddns_getver()
												);
			if ( length >= _countof(user_agent) )
			{
				error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
			}
			else
			{
				if ( 0 == http_add_header(	request,
											"User-Agent",
											user_agent, 1) )
				{
					error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
				}
			}
		}
	}

	/**
	 *	Step 5: Send HTTP request to DNSPod server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int http_status = http_send_request(request, cmd, strlen(cmd));
		if ( 302 == http_status )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
		else if ( (http_status < 200) || (300 <= http_status)  )
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

	/**
	 *	Step 6: Wait for response from DNSPod server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		int result = http_get_response( request,
										(http_callback)&dnspod_http2json,
										json_ctx
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
		else
		{
			if ( NULL != json_value )
			{
				(*json_value) = json_get_value(json_ctx);
			}
		}
	}

	/**
	 *	Step 7: check if valid json expression is returned from server.
	 */
	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		if ( 0 != json_finalize_context(json_ctx) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
	}

	/**
	 *	Step 8: Clean up.
	 */
	json_destroy_context(json_ctx);
	json_ctx	= NULL;
	http_destroy_request(request);
	request		= NULL;

	return error_code;
}


/**
 *	HTTP callback function, it will be called for each received byte and
 *	transfer to byte to json parser.
 */
static void dnspod_http2json(
	char						chr,
	struct json_context		*	context
	)
{
	if ( NULL != context )
	{
		json_readchr(context, chr);
	}
}


/**
 *	HTTP callback function, it will be called for each received byte and
 *	keep the received bytes.
 */
static void dnspod_http2buffer(
	char						chr,
	struct dnspod_buffer	*	buffer
	)
{
	if ( (NULL != buffer) && (NULL != buffer->buffer) )
	{
		if ( buffer->used < buffer->size )
		{
			buffer->buffer[buffer->used] = chr;
			++(buffer->used);
		}
	}
}


/**
 *	Get domain information from the domain name list.
 *
 *	@param[in]	domain_list :	domain name list
 *	@param[in]	domain_name :	the domain name to be searched
 *
 *	@note		Ex.: [domain_name] = "www.website.com", it will search for
 *				domain "website.com"
 *
 *	@return		Find domain and return information of the domain in the domain
 *				list. If it's not found, NULL will be returned.
 */
static const struct dnspod_domain * dnspod_find_domain(
	const struct dnspod_domain	*	domain_list,
	const char					*	domain_name
	)
{
	const struct dnspod_domain * domain = NULL;

	/* Get domain of the domain name (exclude host name) */
	if ( NULL != domain_name )
	{
		domain_name = ddns_strcasestr(domain_name, ".");
		if ( NULL != domain_name )
		{
			++domain_name;
		}
	}

	/* Search domain name in the domain list */
	if ( NULL != domain_name )
	{
		for ( ; NULL != domain_list; domain_list = domain_list->next )
		{
			if ( 0 == ddns_strcasecmp(domain_list->domain, domain_name) )
			{
				domain = domain_list;
				break;
			}
		}
	}

	return domain;
}


/**
 *	Get DNS record information from the DNS record list.
 *
 *	@param[in]	host_list	:	domain name list
 *	@param[in]	record_type :	type of the record searching for.
 *	@param[in]	domain_name :	the domain name to be searched for.
 *
 *	@note		Ex.: if [domain_name] = "www.website.com", if will search for
 *				host "www".
 *
 *	@return		Find DNS host and return information of the host in the host
 *				list. If it's not found, NULL will be returned.
 */
static const struct dnspod_record * dnspod_find_record(
	const struct dnspod_record	*	host_list,
	enum dnspod_record_type			record_type,
	const char					*	domain_name
	)
{
	const struct dnspod_record	*	host	= NULL;
	const char					*	domain	= NULL;
	char							host_name[256];

	memset(host_name, 0, sizeof(host_name));

	/* Get host name from domain name */
	if ( NULL != domain_name )
	{
		domain = ddns_strcasestr(domain_name, ".");
		if ( NULL != domain )
		{
			if ( ((size_t)(domain - domain_name + 1)) < _countof(host_name) )
			{
				c99_strncpy(host_name,
							domain_name,
							domain - domain_name + 1 );
			}
			else
			{
				c99_strncpy(host_name,
							domain_name,
							_countof(host_name) );
			}
		}
	}

	if ( NULL != domain )
	{
		for ( ; NULL != host_list; host_list = host_list->next )
		{
			if (	(DNSPOD_RECORD_TYPE_ALL != host_list->type)
				&&	(record_type != host_list->type) )
			{
				continue;
			}

			if ( 0 == ddns_strcasecmp(host_list->name, host_name) )
			{
				host = host_list;
				break;
			}
		}
	}

	return host;
}


/**
 *	Get internet IP address of local computer.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out] text_buffer : buffer to save the IP address.
 *	@param[in[	buffer_size : size of the buffer.
 *
 *	@note		The returned IP address is like "123.123.123.123".
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successfully get the internet
 *				IP address, otherwise return an error code.
 */
static ddns_error dnspod_get_ip_address(
	struct ddns_context *	context,
	char				*	text_buffer,
	int						buffer_size
	)
{
	struct dnspod_getip_table_entry FUNC_TABLE[] =
	{
#if defined(DNSPOD_USE_DNSPOD) && DNSPOD_USE_DNSPOD > 0
		{ "DNSPod", "http://www.dnspod.cn/About/IP",		DNSPOD_USE_DNSPOD,	&dnspod_get_ip_address_dnspod	},
#endif
#if defined(DNSPOD_USE_BAIDU) && DNSPOD_USE_BAIDU > 0
		{ "Baidu",	"http://www.baidu.com/s?wd=ip",			DNSPOD_USE_BAIDU,	&dnspod_get_ip_address_baidu	},
#endif
#if defined(DNSPOD_USE_IP138) && DNSPOD_USE_IP138 > 0
		{ "IP138",	"http://iframe.ip138.com/ipcity.asp",	DNSPOD_USE_IP138,	&dnspod_get_ip_address_ip138	},
#endif
		{ NULL,		NULL,									INT_MAX,			NULL							}
	};

	struct dnspod_buffer	buffer;
	struct http_request *	request		= NULL;
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;
	char					server_buffer[1024 * 100];
	int						func_idx	= 0;

	/**
	 *	Step 1: argument validity check.
	 */
	if ( (NULL == context) || (NULL == text_buffer) || (buffer_size < 16) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	/**
	 *	Step 2: sort list of servers for getting IP address.
	 */
	qsort(FUNC_TABLE, sizeof(FUNC_TABLE)/sizeof(FUNC_TABLE[0]) - 1, sizeof(FUNC_TABLE[0]), (void*)&dnspod_compare_entry);

	/**
	 *	Step 3: get IP address.
	 */
	error_code = DDNS_ERROR_NO_SERVER;
	for (func_idx = 0; func_idx < sizeof(FUNC_TABLE)/sizeof(FUNC_TABLE[0]); ++func_idx)
	{
		int retry_count = 1;
		char url[DNSPOD_MAX_URL_LENGTH];

		if (NULL == FUNC_TABLE[func_idx].name)
		{
			break;
		}

		c99_strncpy(url, FUNC_TABLE[func_idx].url, sizeof(url));

		ddns_printf_v(context, msg_type_info, "[%s] ", FUNC_TABLE[func_idx].name);

		for ( ; retry_count >= 0; --retry_count)
		{
			memset(&buffer, 0, sizeof(buffer));
			memset(text_buffer, 0, buffer_size);
			memset(&server_buffer, 0, sizeof(server_buffer));
			buffer.buffer	= server_buffer;
			buffer.used		= 0;
			buffer.size		= sizeof(server_buffer);

			error_code = DDNS_ERROR_SUCCESS;

			/**
			 *	Step 2.1: construct HTTP request.
			 */
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				request = http_create_request(	http_method_get,
												url,
												context->timeout
												);
				if ( NULL == request )
				{
					error_code = DDNS_ERROR_BADURL;
				}
				else
				{
					http_set_option(request, HTTP_OPTION_REDIRECT, 5);
				}
			}

			/**
			 *	Step 2.2: connect to server.
			 */
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				int result = http_connect(request);
				switch( result )
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

			/**
			 *	Step 2.3: send request to server via HTTP.
			 */
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				int http_status = http_send_request(request, NULL, 0);
				if ( 302 == http_status )
				{
					error_code = DDNS_ERROR_BADSVR;
				}
				else if ( (http_status < 200) || (http_status >= 300) )
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

			/**
			 *	Step 2.4: get response from server via HTTP.
			 */
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				int result = http_get_response(request,
											   (http_callback)&dnspod_http2buffer,
											   &buffer
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

			/**
			 *	Step 2.5: parse IP address from server response.
			 */
			if (DDNS_ERROR_SUCCESS == error_code)
			{
				error_code = (*FUNC_TABLE[func_idx].func)(&buffer, text_buffer, buffer_size);
			}

			/**
			 *	Step 2.6: IP address validity check.
			 */
			if ( DDNS_ERROR_SUCCESS == error_code )
			{
				int d1 = 0;
				int d2 = 0;
				int d3 = 0;
				int d4 = 0;

				if ( '\0' != text_buffer[0] )
				{
					char extra_chr = 0;
					if ( 4 != sscanf(text_buffer, "%d.%d.%d.%d%c", &d1, &d2, &d3, &d4, &extra_chr) )
					{
						d1 = 0;
						d2 = 0;
						d3 = 0;
						d4 = 0;
					}
				}

				/* If it's a valid IP address? */
				if (	(d1 < 0) || (d1 >= 256) || (d2 < 0) || (d2 >= 256)
					||	(d3 < 0) || (d3 >= 256) || (d4 < 0) || (d4 >= 256)
					||	((0 == d1) && (0 == d2) && (0 == d3) && (0 == d4))
					||	((255 == d1) && (255 == d2) && (255 == d3) && (255 == d4)) )
				{
					error_code = DDNS_ERROR_BADSVR;
				}

				/* If it's a intranet address? */
				if (	(10 == d1)
					||	((172 == d1) && (16 <= d2) && (31>= d2))
					||	((192 == d1) && (168 == d2) && (0 == d3))
					||	((169 == d1) && (254 == d2)) )
				{
					error_code = DDNS_ERROR_BADSVR;
				}
			}

			if (DDNS_ERROR_SUCCESS == error_code)
			{
				break;
			}
			else if (DDNS_ERROR_BADSVR == error_code)
			{
				break;
			}
			else
			{
				if (DDNS_ERROR_REDIRECT == error_code)
				{
					memset(url, 0, sizeof(url));
					c99_strncpy(url, server_buffer, buffer.used < sizeof(url) ? buffer.used : sizeof(url));
				}

				http_destroy_request(request);
				request = NULL;
			}

		}	/* for ( ; retry_count > 0; --retry_count) */

		if (DDNS_ERROR_SUCCESS == error_code)
		{
			break;
		}

	}	/* for (func_idx = 0; ... */

	/**
	 *	Step 4: clean up
	 */
	http_destroy_request(request);
	request = NULL;

	return error_code;
}

/**
 *	Compare priority of 2 servers for getting IP address.
 *
 *	@param[in]	entry1	: the first server record
 *	@param[in]	entry2	: the second server record
 *
 *	@return Return the compare result of the 2 records:
 *				entry1 < entry2: negative
 *				entry1 = entry2: 0
 *				entry1 > entry2: positive
 */
static int dnspod_compare_entry(
	const struct dnspod_getip_table_entry * entry1,
	const struct dnspod_getip_table_entry * entry2
	)
{
	if (entry1->priority < entry2->priority)
	{
		return -1;
	}
	else if (entry1->priority == entry2->priority)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}

/**
 *	Parse server response when getting IP address from server.
 *
 *	@param[in/out]	server_buffer	: server response
 *	@param[out]		text_buffer		: parsed ip address
 *	@param[in]		buffer_size		: size of text_buffer in bytes
 *
 *	@return The following value may be returned:
 *				- DDNS_ERROR_SUCCESS:	legal server response is detected
 *				- DDNS_ERROR_REDIRECT:	redirection is required, redirect URL
 *										is saved in server_buffer.
 *				- Others:				error occurred.
 */
static ddns_error dnspod_get_ip_address_dnspod(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	)
{
	ddns_error error_code = DDNS_ERROR_SUCCESS;

	if (NULL == server_buffer || NULL == server_buffer->buffer || NULL == text_buffer)
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		int length = server_buffer->used + 1;
		if (length > buffer_size)
		{
			length = buffer_size;
		}

		c99_strncpy(text_buffer, server_buffer->buffer, length);
	}

	return error_code;
}

/**
 *	Parse server response when getting IP address from server.
 *
 *	@param[in/out]	server_buffer	: server response
 *	@param[out]		text_buffer		: parsed ip address
 *	@param[in]		buffer_size		: size of text_buffer in bytes
 *
 *	@return The following value may be returned:
 *				- DDNS_ERROR_SUCCESS:	legal server response is detected
 *				- DDNS_ERROR_REDIRECT:	redirection is required, redirect URL
 *										is saved in server_buffer.
 *				- Others:				error occurred.
 */
static ddns_error dnspod_get_ip_address_baidu(
	struct dnspod_buffer	*	server_buffer,
	char					*	text_buffer,
	int							buffer_size
	)
{
	static const char signature[] = ",query:'ip',key:'";

	ddns_error error_code = DDNS_ERROR_SUCCESS;

	if (NULL == server_buffer || NULL == server_buffer->buffer || NULL == text_buffer)
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else
	{
		const char * str = NULL;
		if (server_buffer->used < server_buffer->size)
		{
			server_buffer->buffer[server_buffer->used] = 0;
		}
		else
		{
			server_buffer->buffer[server_buffer->used - 1] = 0;
		}

		str = strstr(server_buffer->buffer, signature);
		if (NULL != str)
		{
			const char * ip = strpbrk(str, "123456789");
			if ((NULL == ip) || (ip != (str + _countof(signature) - 1)))
			{
				error_code = DDNS_ERROR_BADSVR;
			}
			else
			{
				str = ip;
			}
		}
		if (NULL != str)
		{
			const char * end = strstr(str, "'");
			if (NULL == end)
			{
				error_code = DDNS_ERROR_BADSVR;
			}
			else
			{
				int length = length = end - str + 1;
				if (length > buffer_size)
				{
					length = buffer_size;
				}

				c99_strncpy(text_buffer, str, length);
			}
		}
	}

	return error_code;
}

/**
 *	Parse server response when getting IP address from server.
 *
 *	@param[in/out]	server_buffer	: server response
 *	@param[out]		text_buffer		: parsed ip address
 *	@param[in]		buffer_size		: size of text_buffer in bytes
 *
 *	@return The following value may be returned:
 *				- DDNS_ERROR_SUCCESS:	legal server response is detected
 *				- DDNS_ERROR_REDIRECT:	redirection is required, redirect URL
 *										is saved in server_buffer.
 *				- Others:				error occurred.
 */
static ddns_error dnspod_get_ip_address_ip138(
	struct dnspod_buffer		*	server_buffer,
	char						*	text_buffer,
	int								buffer_size
	)
{
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;
	static const char		signature[] = "location.href=\"";
	char				*	start		= NULL;
	char				*	end			= NULL;
	int						length		= 0;

	start = strstr(server_buffer->buffer, signature);
	if (NULL != start)
	{
		start += strlen(signature);
		end = strchr(start, '\"');
		if (NULL != end)
		{
			length = end - start;

			if (0 == ddns_strncasecmp(start, "http://", 7))
			{
				memmove(server_buffer->buffer, start, length);
				server_buffer->buffer[length] = 0;
				server_buffer->used = length + 1;
			}
			else
			{
				static const char prefix[] = "http://www.ip138.com/";
				memmove(server_buffer->buffer + sizeof(prefix) - 1, start, length);
				memcpy(server_buffer->buffer, prefix, sizeof(prefix) - 1);
				server_buffer->buffer[length] = 0;
				server_buffer->used = sizeof(prefix) + length;
			}
		}

		error_code = DDNS_ERROR_REDIRECT;
	}
	else
	{
		start	= strchr(server_buffer->buffer, '[');
		end		= strchr(server_buffer->buffer, ']');
		length	= end - start;
		if ((NULL == start) || (NULL == end) || (length <= 0))
		{
			error_code = DDNS_ERROR_CONNECTION;
		}
		else
		{
			if (length > buffer_size)
			{
				length = buffer_size;
			}

			c99_strncpy(text_buffer, start + 1, length);
		}
	}


	return error_code;
}

/**
 *	Add information of a domain into a domain list.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_info : the domain to be added to the domain list.
 *	@param[out] domain_list : pointer to the domain list.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] on success, otherwise an error
 *				code will be returned.
 */
static ddns_error dnspod_domain_list_push(
	struct ddns_context		*	context,
	struct json_value		*	domain_info,
	struct dnspod_domain	**	domain_list
	)
{
	struct json_value *		id			= NULL;
	struct json_value *		name		= NULL;
	struct json_value *		status		= NULL;
	struct json_value *		records		= NULL;
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;

	if ( (NULL == domain_info) || (NULL == domain_list) )
	{
		error_code = DDNS_ERROR_BADARG;
	}
	else if ( json_type_object != json_get_type(domain_info) )
	{
		error_code = DDNS_ERROR_BADSVR;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		id		= json_object_get(domain_info, "id");
		name	= json_object_get(domain_info, "name");
		status	= json_object_get(domain_info, "status");
		records = json_object_get(domain_info, "records");

		if ( 0 == json_to_number(id) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
		if ( json_type_string != json_get_type(name) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
		if ( json_type_string != json_get_type(status) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
		if ( json_type_string != json_get_type(records) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}

		if ( DDNS_ERROR_SUCCESS == error_code )
		{
			struct dnspod_domain * domain = NULL;

			domain = (struct dnspod_domain*)malloc(sizeof(*domain));
			if ( NULL != domain )
			{
				memset(domain, 0, sizeof(*domain));

				domain->next		= *domain_list;
				domain->domain_id	= (int)json_number_get(id);
				c99_strncpy(domain->domain,
							json_string_get(name),
							_countof(domain->domain) );

				error_code = dnspod_get_domain_priv(context, domain);
				if (DDNS_ERROR_SUCCESS == error_code)
				{
					(*domain_list) = domain;
				}
				else
				{
					domain->next = NULL;
					dnspod_destroy_domain_list(domain);
				}
			}
			else
			{
				error_code = DDNS_ERROR_INSUFFICIENT_MEMORY;
			}
		}

		json_destroy(id);		id		= NULL;
		json_destroy(name);		name	= NULL;
		json_destroy(status);	status	= NULL;
		json_destroy(records);	records = NULL;
	}

	return error_code;
}


/**
 *	Get human readable name of a record type.
 *
 *	@param[in]	type			: the DNS record type.
 *
 *	@return		Return pointer to the statically allocated type name.
 */
static const char * dnspod_record_type_name(enum dnspod_record_type type)
{
	const char * name = NULL;

	switch (type)
	{
	case DNSPOD_RECORD_TYPE_A:
		name = "A";
		break;

	case DNSPOD_RECORD_TYPE_CNAME:
		name = "CNAME";
		break;

	case DNSPOD_RECORD_TYPE_MX:
		name = "MX";
		break;

	case DNSPOD_RECORD_TYPE_URL:
		name = "URL";
		break;

	case DNSPOD_RECORD_TYPE_NS:
		name = "NS";
		break;

	case DNSPOD_RECORD_TYPE_TXT:
		name = "TXT";
		break;

	case DNSPOD_RECORD_TYPE_AAAA:
		name = "AAAA";
		break;

	default:
		name = "UNKNOWN";
		break;
	}

	return name;
}


/**
 *	Convert human readable name of record type to [dnspod_record_type].
 *
 *	@param		type			: DNS record type
 *
 *	@return		Type of the DNS record.
 */
static enum dnspod_record_type dnspod_get_record_type(const char * type)
{
	enum dnspod_record_type record_type = DNSPOD_RECORD_TYPE_ALL;

	if ( NULL == type )
	{
		/* invalid type: do nothing */
	}
	else if ( 0 == ddns_strcasecmp("A", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_A;
	}
	else if ( 0 == ddns_strcasecmp("CNAME", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_CNAME;
	}
	else if ( 0 == ddns_strcasecmp("MX", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_MX;
	}
	else if ( 0 == ddns_strcasecmp("URL", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_URL;
	}
	else if ( 0 == ddns_strcasecmp("NS", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_NS;
	}
	else if ( 0 == ddns_strcasecmp("TXT", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_TXT;
	}
	else if ( 0 == ddns_strcasecmp("AAAA", type) )
	{
		record_type = DNSPOD_RECORD_TYPE_AAAA;
	}
	else
	{
		/* unknown type: do nothing */
	}

	return record_type;
}


/**
 *	Get human readable name of a record line.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	type		: the DNS record line.
 *
 *	@return		Return pointer to the statically allocated line name (utf-8).
 */
static const char * dnspod_record_line_name(
	const struct ddns_context	*	context,
	enum dnspod_record_line			line
	)
{
	const char				*	name	= "unknown";
	struct dnspod_context	*	dnspod	= NULL;

	if ( (NULL == context) || (NULL == context->extra_data) )
	{
		/* should never reach here */
		assert(0);
	}
	else
	{
		int				i			= 0;
		ddns_ulong32	api_version = 0;

		dnspod = (struct dnspod_context*)(context->extra_data);
		for ( i = 0; i < _countof(dnspod_record_line_table); ++i )
		{
			if ( dnspod_record_line_table[i].api_version > dnspod->api_version)
			{
				/* higher api version required */
				continue;
			}

			if ( dnspod_record_line_table[i].line == line )
			{
				if ( dnspod_record_line_table[i].api_version > api_version )
				{
					name		= dnspod_record_line_table[i].name;
					api_version = dnspod_record_line_table[i].api_version;
				}
			}
		}
	}

	return name;
}


/**
 *	Convert human readable line type to type [dnspod_record_line].
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	line		: line type
 *
 *	@return		Type of the network which your DNS record associated with.
 */
static enum dnspod_record_line dnspod_get_record_line(
	const struct ddns_context	*	context,
	const char					*	line
	)
{
	enum dnspod_record_line		line_type	= DNSPOD_LINE_ALL;
	struct dnspod_context	*	dnspod		= NULL;

	if ( (NULL == context) || (NULL == context->extra_data) || (NULL == line) )
	{
		/* should never reach here */
		assert(0);
	}
	else
	{
		int				i			= 0;
		ddns_ulong32	api_version = 0;

		dnspod = (struct dnspod_context*)(context->extra_data);
		for ( i = 0; i < _countof(dnspod_record_line_table); ++i )
		{
			if ( dnspod_record_line_table[i].api_version > dnspod->api_version)
			{
				/* higher api version required */
				continue;
			}

			if ( 0 == ddns_strcasecmp(line, dnspod_record_line_table[i].name) )
			{
				if ( dnspod_record_line_table[i].api_version > api_version )
				{
					line_type	= dnspod_record_line_table[i].line;
					api_version = dnspod_record_line_table[i].api_version;
				}
			}
		}
	}

	return line_type;
}


/**
 *	Get API version of the DNSPod server.
 *
 *	@param[in/out]	context		: the DDNS context.
 *
 *	@return		Higher 2 bytes represents the major version, lower 2 bytes
 *				represents as the minor version. If any error occurred, 0 will
 *				be returned.
 */
static ddns_error dnspod_get_api_version(
	struct ddns_context		*	context,
	ddns_ulong32			*	api_version
	)
{
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;
	struct json_value	*	json		= NULL;
	char					command[512];

	if ( (NULL == context) || (NULL == api_version) )
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
        size_t  length = 0;
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_API_VERSION,
										username,
										password
										);
		if ( length >= _countof(command) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_send_command(	context,
											DNSPOD_API_VERSION,
											command,
											&json
											);
		if (	(DDNS_ERROR_SUCCESS == error_code)
			&&	(json_type_object != json_get_type(json)) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_error err = dnspod_handle_common_error(json, &error_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;
		case DDNS_ERROR_NOTIMPL:
			error_code = DDNS_ERROR_UNKNOWN;
			break;
		default:
			error_code = err;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		struct json_value	*	status	= json_object_get(json, "status");
		struct json_value	*	message = json_object_get(status, "message");
		if (json_type_string == json_get_type(message))
		{
			char			*	decimal		= NULL;
			int					major_ver	= 0;
			int					minor_ver	= 0;

			major_ver = strtoul(json_string_get(message), &decimal, 10);
			if ( '.' != (*decimal) )
			{
				error_code = DDNS_ERROR_BADSVR;
			}
			else
			{
				minor_ver = strtoul(decimal + 1, NULL, 10);
				*api_version = major_ver * 0x10000 + minor_ver;
			}
		}
		else
		{
			error_code = DDNS_ERROR_BADSVR;
		}

		json_destroy(message);	message = NULL;
		json_destroy(status);	status = NULL;
	}

	return error_code;
}

/**
 *	Retrieve additional information of a domain that's not provided in domain
 *	list by default.
 *
 *	@param[in/out]	context		: the DDNS context.
 *	@param[in/out]	domain		: the domain to be handled.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if every thing's going fine.
 *				Otherwise a error code will be returned.
 *
 */
static ddns_error dnspod_get_domain_priv(
	struct ddns_context		*	context,
	struct dnspod_domain	*	domain
	)
{
	ddns_error				error_code	= DDNS_ERROR_SUCCESS;
	struct json_value	*	json		= NULL;
	char					command[1024];

	if (NULL == context || NULL == domain || 0 == domain->domain_id)
	{
		error_code = DDNS_ERROR_BADARG;
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		size_t length = 0;
		char	username[sizeof(context->username) * 3];
		char	password[sizeof(context->password) * 3];

		http_urlencode(context->username, username, _countof(username));
		http_urlencode(context->password, password, _countof(password));
		length = c99_snprintf(command,	_countof(command),
										DNSPOD_CMD_DOMAIN_PRIV,
										username,
										password,
										domain->domain_id
										);
		if ( length >= _countof(command) )
		{
			error_code = DDNS_ERROR_INSUFFICIENT_BUFFER;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		error_code = dnspod_send_command(	context,
											DNSPOD_DOMAIN_PRIV,
											command,
											&json
											);
		if (	(DDNS_ERROR_SUCCESS == error_code)
			&&	(json_type_object != json_get_type(json)) )
		{
			error_code = DDNS_ERROR_BADSVR;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		ddns_error err = dnspod_handle_common_error(json, &error_code);
		switch (err)
		{
		case DDNS_ERROR_SUCCESS:
			break;

		case DDNS_ERROR_NOTIMPL:
			switch(error_code)
			{
			case 6:
				error_code = DDNS_ERROR_NXDOMAIN;
				break;
			default:
				error_code = DDNS_ERROR_UNKNOWN;
				break;
			}
			break;

		default:
			error_code = err;
			break;
		}
	}

	if ( DDNS_ERROR_SUCCESS == error_code )
	{
		static const char signature[] = "\xe8\xae\xb0\xe5\xbd\x95"
										"TTL\xe6\x9c\x80\xe4\xbd\x8e";
		struct json_value * purview = json_object_get(json, "purview");

		if (json_type_array != json_get_type(purview))
		{
			error_code = DDNS_ERROR_BADSVR;
		}
		else
		{
			int idx = 0;
			int cnt = json_array_size(purview);
			for (idx = 0; idx < cnt; ++idx)
			{
				struct json_value * pair = json_array_get(purview, idx);
				struct json_value * name = json_object_get(pair, "name");
				struct json_value * value = json_object_get(pair, "value");
				if (json_type_string != json_get_type(name))
				{
					error_code = DDNS_ERROR_BADSVR;
					break;
				}
				if (0 == ddns_strcasecmp(json_string_get(name), signature))
				{
					if (0 == json_to_number(value))
					{
						error_code = DDNS_ERROR_BADSVR;
					}
					else
					{
						domain->min_ttl = (int)json_number_get(value);
					}
					break;
				}
			}
		}
	}

	return error_code;
}

/**
 *	Convert common server error code into ours.
 *
 *	@param[in]	json		: the json object returned from server.
 *	@param[out]	error_code	: the converted error code.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if it's recognized as succesful
 *				or any known error, in such case, [error_code] will be set to
 *				the converted error code. Otherwise [DDNS_ERROR_UNKNOWN] will be
 *				returned and [error_code] will be set to the raw error code
 *				returned from server.
 */
static ddns_error dnspod_handle_common_error(
	struct json_value			*	json,
	ddns_error					*	error_code
	)
{
	ddns_error err = DDNS_ERROR_SUCCESS;

	if (NULL == json || NULL == error_code)
	{
		err = DDNS_ERROR_BADARG;
	}
	else if (json_type_object != json_get_type(json))
	{
		err = DDNS_ERROR_BADSVR;
	}
	else
	{
		struct json_value	*	status	= json_object_get(json, "status");
		struct json_value	*	code	= NULL;

		if ( json_type_object != json_get_type(status) )
		{
			err = DDNS_ERROR_BADSVR;
		}

		if ( DDNS_ERROR_SUCCESS == err )
		{
			code = json_object_get(status, "code");
			if ( json_type_string != json_get_type(code) )
			{
				err = DDNS_ERROR_BADSVR;
			}
		}

		if ( DDNS_ERROR_SUCCESS == err )
		{
			ddns_ulong32 status_code = strtoul(json_string_get(code), NULL, 10);
			switch (status_code)
			{
			case -99:	/* API closed temporarily, try later */
				*error_code = DDNS_ERROR_SVRDOWN;
				break;
			case -8:	/* Login failed too many times, blocked temporarily */
				*error_code = DDNS_ERROR_BLOCKED;
				break;
			case -7:	/* Not allowed to use this API */
				*error_code = DDNS_FATAL_ERROR(DDNS_ERROR_PAIDFEATURE);
				break;
			case -4:	/* Account not under the agent (agent API only) */
				*error_code = DDNS_ERROR_UNKNOWN;
				break;
			case -3:	/* Illegal agent (agent API only) */
				*error_code = DDNS_ERROR_UNKNOWN;
				break;
			case -2:	/* Exceed the maximum allowed usage (API) */
				*error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BLOCKED);
				break;
			case -1:	/* login failed */
				*error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADAUTH);
				break;
			case 1:		/* Good */
				*error_code = DDNS_ERROR_SUCCESS;
				break;
			case 2:		/* POST method only */
				*error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADAGENT);
				break;
			case 3:		/* Unknown error */
				*error_code = DDNS_ERROR_UNKNOWN;
				break;
			case 85:	/* Login remotely, rejected */
				*error_code = DDNS_FATAL_ERROR(DDNS_ERROR_BADAUTH);
				break;
			case 6:		/* Illegal user id (agent API only) */
			case 7:		/* Account not under the agent (agent API only) */
			default:
				err = DDNS_ERROR_NOTIMPL;
				*error_code = status_code;
				break;
			}
		}

		json_destroy(code);		code = NULL;
		json_destroy(status);	status = NULL;
	}

	return err;
}
