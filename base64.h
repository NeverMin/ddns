/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: base64.h 261 2011-05-15 15:54:36Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2008-03-02.
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
 *  Provide base-64 encoding/decoding support.
 */

#ifndef _INC_DDNS_BASE64
#define _INC_DDNS_BASE64

#ifdef __cplusplus
extern "C" {
#endif
	
/*
 *	Encode a stream in base-64 algorithm.
 *
 *	@param[in]	pBase64		: the binary stream to be encoded as base-64 string.
 *	@param[in]	nLen		: length of the binary stream.
 *	@param[out]	pOutBuf		: the base-64 encoded string (null terminated).
 *	@param[in]	nBufSize	: size of the output buffer, in bytes.
 *
 *	@return	If successful, it will return count of bytes wrote into the output
 *			buffer including the null terminator. Otherwise, it will return the
 *			minimum required buffer size.
 */
int base64_encode(unsigned char* pInBuf, int nLen, char* pszOut, int nBufSize);

/*
 *	Decode a base-64 encoded string.
 *
 *	@param[in]	pszIn		: a base-64 encoded string (null terminated).
 *	@param[out]	pszOut		: the decoded binary stream.
 *	@param[in]	nBufSize	: size of the output buffer in bytes.
 *
 *	@return	If successful, it will return count of bytes wrote into the output
 *			buffer. Otherwise, it will return the minimum required buffer size.
 */
int base64_decode(char* pInBuf, unsigned char* pOutBuf, int nBufSize);

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* !defined(_INC_DDNS_BASE64) */
