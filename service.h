/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: service.h 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-09-23.
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
 *  Interact with NT-Service manager. This file is for Windows only.
 */

#ifndef _INC_DDNS_SERVICE
#define _INC_DDNS_SERVICE

#include "ddns.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Main entry for starting the DDNS service.
 */
int ddns_nt_service(struct ddns_context* context);

/**
 *	Install the tool as windows service.
 *
 *	@param[in]	service_name	: name of windows service
 *	@param[in]	config_file		: path to the configuration file
 *	@param[in]	user_name		: user to run the service
 *	@param[in]	password		: password of the user to run the service
 *
 *	@note		if [config_file] is NULL, "ddns.ini" will be used.
 *	@note		if [config_file] is not a absolute path, we will consider it's
 *				relative to the program's parent folder.
 *
 *	@return		Return [ERROR_SUCCESS] on success, otherwise a WIN32 error code
 *				is returned.
 */
int ddns_nt_service_install(
	struct ddns_context	*	context,
	const char			*	service_name,
	const char			*	config_file,
	const char			*	user_name,
	const char			*	password
	);

#ifdef __cplusplus
}
#endif

#endif	/* _INC_DDNS_SERVICE */
