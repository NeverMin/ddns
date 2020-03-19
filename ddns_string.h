/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_string.h 269 2012-02-15 16:36:31Z kuang $
 *
 *	This file is part of 'ddns', Created by karl on 2009-09-25.
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
 *  Provide ISO-C99 compatible string functions. For compilers that
 *	compatible with ISO-C99 standard, it just simply redirect the calls
 *	to C runtime. This file presents here because of some brain-dead
 *	compilers.
 */

#ifndef _INC_DDNS_STRING
#define _INC_DDNS_STRING

#include <stddef.h>		/* ISO-C89: size_t  */
#include <stdarg.h>		/* ISO-C89: va_list */

#ifdef __cplusplus
extern "C" {
#endif

typedef char XCHAR;

#define _xchar_inc(str)	(++(str))

/**
 *	Return element count of a array.
 *
 *	@note: Do *NOT* use it with pointers, you won't get a correct return!
 */
#ifndef _countof
#	define _countof(arg)	( sizeof(arg) / sizeof((arg)[0]) )
#endif


/**
 *	ISO-C99 "strncpy" replacement.
 */
XCHAR * c99_strncpy(XCHAR *dst, const XCHAR *src, size_t size);


/**
 *	ISO-C99 compatible "strncat" replacement, concatenate strings.
 *
 *	@param[out]		dst		: the string to be appended to.
 *	@param[in]		src		: the string to be appended.
 *	@param[in]		size	: size of [dst] in characters.
 *
 *	@return		It returns the value of [dst].
 */
XCHAR * c99_strncat(XCHAR *dst, const XCHAR *src, size_t size);


/**
 *	ISO-C99 compatible "snprintf" replacement, output formatted string.
 *
 *	@param[out]		str		: storage location for output.
 *	@param[in]		size	: maximum number of characters to write.
 *	@param[in]		format	: format specification.
 *	@param[in]		...		: optional arguments
 *
 *	@return		Number of characters (exclude '\0') should have been wrote to
 *				[str] if it's sufficiently large.
 */
int c99_snprintf(XCHAR *str, size_t size, const XCHAR *format, ...);


/**
 *	ISO-C99 compatible "vsnprintf" replacement, output formatted string.
 *
 *	@param[out]		str		: storage location for output.
 *	@param[in]		size	: maximum number of characters to write.
 *	@param[in]		format	: format specification.
 *	@param[in]		ap		: argument list
 *
 *	@return		Number of characters (exclude '\0') should have been wrote to
 *				[str] if it's sufficiently large.
 */
int c99_vsnprintf(XCHAR *str, size_t size, const XCHAR *format, va_list ap);


#ifdef __cplusplus
};	/* extern "C" */
#endif

#endif	/* _INC_DDNS_STRING */
