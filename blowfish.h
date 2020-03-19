/*
 blowfish.h:  Header file for blowfish.c
 
 Copyright (C) 1997 by Paul Kocher
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
 
 See blowfish.c for more information about this file.
 */

#ifndef _INC_BLOWFISH
#define _INC_BLOWFISH

#include "ddns_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	ddns_ulong32 P[16 + 2];
	ddns_ulong32 S[4][256];
} BLOWFISH_CTX;


/*
 *	Initialize a Blowfish context using a private key.
 *
 *	@param[in]	ctx		: pointer to the Blowfish context to be initialized.
 *	@param[in]	key		: pointer to the private key
 *	@param[in]	keyLen	: length of the private key in bytes.
 */
void Blowfish_Init(
	BLOWFISH_CTX *ctx,
	unsigned char *key,
	int keyLen
	);


/*
 *	Encode a 64 bits block.
 *
 *	@param[in]		ctx	: pointer to a Blowfish context.
 *	@param[in][out]	key	: the first 32 bit data to be encoded.
 *	@param[in][out]	xr	: the last 32 bit data to be encoded.
 */
void Blowfish_Encrypt(
	BLOWFISH_CTX *ctx,
	ddns_ulong32 *xl,
	ddns_ulong32 *xr
	);


/*
 *	Decode a 64 bits block.
 *
 *	@param[in]		ctx	: pointer to a Blowfish context.
 *	@param[in][out]	key	: the first 32 bit data to be decoded.
 *	@param[in][out]	xr	: the last 32 bit data to be decoded.
 */
void Blowfish_Decrypt(
	BLOWFISH_CTX *ctx,
	ddns_ulong32 *xl,
	ddns_ulong32 *xr
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* !defined(_INC_BLOWFISH) */
