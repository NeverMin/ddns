/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: dnspod.h 301 2014-01-24 08:36:50Z kuang $
 *
 *	This file is part of 'ddns', Created by karl on 2009-05-23.
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
 *  Interface for accessing DNSPod service.
 */

#ifndef _INC_DNSPOD
#define _INC_DNSPOD

#include "ddns.h"
#include "ddns_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum dnspod_record_type
{
	DNSPOD_RECORD_TYPE_ALL		= 0,	/* dummy type: all types */
	DNSPOD_RECORD_TYPE_A		= 1,
	DNSPOD_RECORD_TYPE_CNAME	= 2,
	DNSPOD_RECORD_TYPE_MX		= 3,
	DNSPOD_RECORD_TYPE_URL		= 4,
	DNSPOD_RECORD_TYPE_NS		= 5,
	DNSPOD_RECORD_TYPE_TXT		= 6,
	DNSPOD_RECORD_TYPE_AAAA		= 7
};

enum dnspod_record_line
{
	DNSPOD_LINE_ALL		= 1,	/* Common */
	DNSPOD_LINE_CTC		= 2,	/* China Telecome */
	DNSPOD_LINE_CNC		= 3,	/* China NetCom */
	DNSPOD_LINE_EDU		= 4,	/* China Education & Research Network */
	DNSPOD_LINE_CMC		= 5,	/* China Mobile */
	DNSPOD_LINE_CRC		= 6,	/* China Railcom */
	DNSPOD_LINE_UNC		= 7,	/* China Unicom */
	DNSPOD_LINE_FRGN	= 8		/* Foreign */
};

struct dnspod_domain;
struct dnspod_record;

/**
 *	Create DDNS interface for accessing DNSPod service.
 *
 *	@return		Return pointer to the newly created interface object, or NULL
 *				if there's an error creating interface object.
 */
ddns_interface * dnspod_create_interface(void);


/**
 *	Create a domain under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_name	: name of the domain to be created.
 *	@param[out]	error_code	: to keep the error code if encountered any error.
 *
 *	@return		Return ID of the newly created domain if successful, otherwise
 *				0 will be returned.
 */
unsigned long dnspod_create_domain(
	const struct ddns_context	*	context,
	const char					*	domain_name,
	ddns_error					*	error_code
	);


/**
 *	Remove a domain from your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain_name	: name of the domain to be removed.
 *
 *	@return		Return [DNSPOD_ERROR_SUCCESS] if successful, otherwise return
 *				an error code.
 */
ddns_error dnspod_remove_domain_by_name(
	struct ddns_context		*	context,
	const char				*	domain_name
	);


/**
 *	List registered domains under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[out]	error_code	: to keep the error code if encountered any error.
 *
 *	@note		It's safe to pass NULL to [error_code].
 *
 *	@return		Return pointer to the domain list, when the domain list is no
 *				longer needed, use [dnspod_destroy_domain_list] to free it.
 */
struct dnspod_domain * dnspod_list_domain(
	struct ddns_context		*	context,
	ddns_error				*	error_code
	);


/**
 *	Destroy & free resources allocated for a domain list.
 *
 *	@param	list		: the domain list to be freed.
 */
void dnspod_destroy_domain_list(struct dnspod_domain * list);


/**
 *	List DNS record for a domain under your account.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	domain		: the domain to be listed.
 *	@param[out]	error_code	: to keep the error code if encountered any error.
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
	);


/**
 *	Destroy & free resources allocated for a record list.
 *
 *	@param	list		: the record list to be freed.
 */
void dnspod_destroy_record_list(struct dnspod_record * list);
	
	
/**
 *	Update address of a one DNS record.
 *
 *	@param[in]		context		: the DDNS context.
 *	@param[in/out]	domain_list	: domain list under your DNSPod account.
 *	@param[in]		domain_name	: the DNS record to be updated.
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
	);

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
	);

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
	);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* _INC_DNSPOD */
