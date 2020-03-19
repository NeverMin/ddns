/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: base64.c 243 2010-07-13 14:10:07Z karl $
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

#include "base64.h"
#include <string.h>

/*
 *	base-64 conversion table.
 */
static char base64_table[64] =
{
	'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N',
	'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't',
	'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'+', '/'
};


/*
 *	Decode a single base-64 encoded character.
 *
 *	@param[in]	nChr	: a single character of a base-64 encoded string.
 *
 *	@return the decoded value corresponding to the base-64 character.
 */
static int base64_decode_char(char nChr)
{
	if ( nChr >= 'A' && nChr <= 'Z' )
	{
		return nChr - 'A';
	}
	else if ( nChr >= 'a' && nChr <= 'z' )
	{
		return nChr - 'a' + 26;
	}
	else if ( nChr >= '0' && nChr <= '9' )
	{
		return nChr - '0' + 52;
	}
	else if ( '+' == nChr )
	{
		return 62;
	}
	else
	{
		return 63;
	}
}


/*
 *	Calculate valid length of a base-64 encoded string.
 *
 *	@param[in]	pszBase64	: the base-64 encoded string (null terminated).
 *
 *	@return	length of the base64 encoded string.
 */
static int base64len(char* pszBase64)
{
	int iLen = 0;
	for ( iLen = 0; pszBase64[iLen]; ++iLen )
	{
		if (	((pszBase64[iLen] < 'a') || (pszBase64[iLen] > 'z'))
			&&	((pszBase64[iLen] < 'A') || (pszBase64[iLen] > 'Z'))
			&&	((pszBase64[iLen] < '0') || (pszBase64[iLen] > '9'))
			&&	(pszBase64[iLen] != '+')
			&&	(pszBase64[iLen] != '/')
			&&	(pszBase64[iLen] != '=') )
		 {
			break;
		 }
	}

	return iLen;
}


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
int base64_decode(char* pszIn, unsigned char* pszOut, int nBufSize)
{
	int nInStrLen = 0;
	int nOutStrLen = 0;
	int i = 0;
	int j = 0;
	int tmp = 0;

	nInStrLen = base64len((char*)pszIn);
	nOutStrLen = nInStrLen / 4 * 3;
	if ( '=' == pszIn[nInStrLen-1] )
	{
		if ( '=' == pszIn[nInStrLen-2] )
		{
			nOutStrLen -= 2;
		}
		else
		{
			nOutStrLen -= 1;
		}
	}

	if ( pszOut && 0 == (nInStrLen % 4) && nBufSize >= nOutStrLen )
	{
		for ( i = 0, j = 0; i < nInStrLen; i += 4, j += 3 )
		{
			/* the first byte: from the first & second characters. */
			tmp = ((base64_decode_char(pszIn[i]) & 0x3f) << 18);
			tmp |= (((base64_decode_char(pszIn[i+1]) & 0x3f) << 12));
			pszOut[j] = (unsigned char)((tmp >> 16) & 0xff);

			/* the second byte: from the second & third characters. */
			if ( '=' != pszIn[i+2] )
			{
				tmp |= ((base64_decode_char(pszIn[i+2]) & 0x3f) << 6);
				pszOut[j+1] = (unsigned char)((tmp >> 8 ) & 0xff);
			}

			/* the third byte: from the third & fourth characters. */
			if ( '=' != pszIn[i+3] )
			{
				tmp |= ((base64_decode_char(pszIn[i+3]) & 0x3f));
				pszOut[j+2] = (unsigned char)(tmp & 0xff);
			}
		}
	}

	return nOutStrLen;
}

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
int base64_encode(unsigned char* pBase64, int nLen, char* pOutBuf, int nBufSize)
{
	int i = 0;
	int j = 0;
	int nOutStrLen = 0;

	/* nOutStrLen does not contain null terminator. */
	nOutStrLen = nLen / 3 * 4 + (0 == (nLen % 3) ? 0 : 4);
	if ( pOutBuf && nOutStrLen < nBufSize )
	{
		char cTmp = 0;
		for ( i = 0, j = 0; i < nLen; i += 3, j += 4 )
		{
			/* the first character: from the first byte. */
			pOutBuf[j] = base64_table[pBase64[i] >> 2];

			/* the second character: from the first & second byte. */
			cTmp = (char)((pBase64[i] & 0x3) << 4);
			if ( i + 1 < nLen )
			{
				cTmp |= ((pBase64[i + 1] & 0xf0) >> 4);
			}
			pOutBuf[j+1] = base64_table[(int)cTmp];

			/* the third character: from the second & third byte. */
			cTmp = '=';
			if ( i + 1 < nLen )
			{
				cTmp = (char)((pBase64[i + 1] & 0xf) << 2);
				if ( i + 2 < nLen )
				{
					cTmp |= (pBase64[i + 2] >> 6);
				}
				cTmp = base64_table[(int)cTmp];
			}
			pOutBuf[j + 2] = cTmp;

			/* the fourth character: from the third byte. */
			cTmp = '=';
			if ( i + 2 < nLen )
			{
				cTmp = base64_table[pBase64[i + 2] & 0x3f];
			}
			pOutBuf[j + 3] = cTmp;
		}

		pOutBuf[j] = '\0';
	}

	return nOutStrLen + 1;
}
