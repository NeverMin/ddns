/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_types.h 253 2010-10-27 15:20:37Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-03-27.
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
 *	Define basic data types used through out the 'ddns' project.
 */

#ifndef _INC_DDNS_TYPES
#define _INC_DDNS_TYPES

#ifdef HAVE_CONFIG_H
#	include "config.h"
#	if defined(HAVE_LIMITS_H) && HAVE_LIMITS_H
#		include <limits.h>
#	endif
#else
#	include <limits.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(SHRT_MAX) && SHRT_MAX == 32767
typedef signed short				ddns_int16;
typedef unsigned short				ddns_uint16;
#else
#	error "Not supported!"
#endif

#if defined(INT_MAX) && INT_MAX == 2147483647
typedef signed int					ddns_int32;
typedef unsigned int				ddns_uint32;
#elif defined(LONG_MAX) && LONG_MAX == 2147483647
typedef signed long int				ddns_int32;
typedef unsigned long int			ddns_uint32;
#else
#	error "Not supported!"
#endif

#if defined(LONG_MAX) && LONG_MAX == 2147483647
#define _DDNS_INT32 1
typedef signed long int				ddns_long32;
typedef unsigned long int			ddns_ulong32;
#elif defined(INT_MAX) && INT_MAX == 2147483647
#define _DDNS_INT32 1
typedef signed int					ddns_long32;
typedef unsigned int				ddns_ulong32;
#else
#	error "Not supported!"
#endif

#if defined(LLONG_MAX) && LLONG_MAX == 9223372036854775807
#define _DDNS_INT64 1
typedef signed long long int		ddns_int64;
typedef unsigned long long int		ddns_uint64;
#elif defined(_MSC_VER) && defined(_I64_MAX) && _I64_MAX == 9223372036854775807
#define _DDNS_INT64 1
typedef signed __int64				ddns_int64;
typedef unsigned __int64			ddns_uint64;
#else
#define _DDNS_INT64 0
#endif


#if defined(_DDNS_INT64) && _DDNS_INT64
typedef ddns_int64					ddns_intmax;
typedef ddns_uint64					ddns_uintmax;
#elif defined(_DDNS_INT32) && _DDNS_INT32
typedef ddns_int32					ddns_intmax;
typedef ddns_uint32					ddns_uintmax;
#endif


#ifdef __cplusplus
}
#endif

#endif	/* _INC_DDNS_TYPES */
