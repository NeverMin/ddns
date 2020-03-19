/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: hmac.h 243 2010-07-13 14:10:07Z karl $
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
 *  Provide HMAC authentication support.
 */

#ifndef _INC_HMAC_HEADER
#define _INC_HMAC_HEADER

#ifdef __cplusplus
extern "C" {
#endif

typedef int(hmac_hash_routine)(
	/* [in] */ unsigned char *text_in,
	/* [in] */ int text_len,
	/* [out] */ unsigned char *digest,
	/* [in] */int digest_len
	);


/**
 *	Calculate HMAC-SHA1 digest.
 *
 *	@param[in]	text		: pointer to data stream.
 *	@param[in]	text_len	: length of data stream.
 *	@param[in]	key			: pointer to authentication key.
 *	@param[in]	key_len		: length of authentication key.
 *	@param[out]	digest		: caller digest to be filled in.
 *	@param[in]	digest_len	: size of output buffer in bytes.
 *
 *	@return	length of the digest in bytes.
 */
int hmac_sha1(
	/* [in] */ unsigned char *text,
	/* [in] */ int text_len,
	/* [in] */ unsigned char *key,
	/* [in] */ int key_len,
	/* [out] */ unsigned char *digest,
	/* [in] */ int digest_len
	);


/**
 *	Calculate HMAC-MD5 digest.
 *
 *	@param[in]	text		: pointer to data stream.
 *	@param[in]	text_len	: length of data stream.
 *	@param[in]	key			: pointer to authentication key.
 *	@param[in]	key_len		: length of authentication key.
 *	@param[out]	digest		: caller digest to be filled in.
 *	@param[in]	digest_len	: size of output buffer in bytes.
 *
 *	@return	length of the digest in bytes.
 */
int hmac_md5(
	/* [in] */ unsigned char *text,
	/* [in] */ int text_len,
	/* [in] */ unsigned char *key,
	/* [in] */ int key_len,
	/* [out] */ unsigned char *digest,
	/* [in] */ int digest_len
	);


/**
 *	Calculate HMAC digest.
 *
 *	@param[in]	text		: pointer to data stream.
 *	@param[in]	text_len	: length of data stream.
 *	@param[in]	key			: pointer to authentication key.
 *	@param[in]	key_len		: length of authentication key.
 *	@param[out]	digest		: caller digest to be filled in.
 *	@param[in]	digest_len	: size of output buffer in bytes.
 *	@param[in]	hash		: pointer to the hash routine.
 *
 *	@return	length of the digest in bytes.
 */
int hmac(
	/* [in] */ unsigned char *text,
	/* [in] */ int text_len,
	/* [in] */ unsigned char *key,
	/* [in] */ int key_len,
	/* [out] */ unsigned char *digest,
	/* [in] */ int digest_len,
	/* [in] */ hmac_hash_routine *hash
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* !defined(_INC_HMAC_HEADER) */

