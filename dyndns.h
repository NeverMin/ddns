/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: dyndns.h 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-12-19.
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

#ifndef _INC_DYNDNS
#define _INC_DYNDNS

#include "ddns.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Create DDNS interface for accessing DynDNS service.
 *
 *	@return		Return pointer to the newly created interface object, or NULL
 *				if there's an error creating interface object.
 */
ddns_interface * dyndns_create_interface(void);

#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif	/* _INC_DYNDNS */
