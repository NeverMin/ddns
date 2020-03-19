/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: hmac.c 243 2010-07-13 14:10:07Z karl $
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

#include "hmac.h"
#include <stdlib.h>
#include <string.h>
#include "sha1.h"
#include "md5.h"


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
int hmac_sha1(	unsigned char*	text,
				int				text_len,
				unsigned char*	key,
				int				key_len,
				unsigned char*	digest,
				int				digest_len )
{
	const int hash_size = 20;

	SHA1Context context;

	int i = 0;
	unsigned char tk[20];
	unsigned char k_ipad[65];	/* inner padding key XORd with ipad */
	unsigned char k_opad[65];	/* outer padding key XORd with opad */

	if ( digest_len >= hash_size )
	{
		/* if key is longer than 64 bytes reset it to key=MD5(key) */
		if ( key_len > 64 )
		{
			MD5_CTX      tctx;

			MD5Init(&tctx);
			MD5Update(&tctx, key, key_len);
			MD5Final(tk, &tctx);

			key = tk;
			key_len = hash_size;
		}

		/*
		 * the HMAC_SHA1 transform looks like:
		 *
		 * SHA1(K XOR opad, SHA1(K XOR ipad, text))
		 *
		 * where K is an n byte key
		 * ipad is the byte 0x36 repeated 64 times
		 * opad is the byte 0x5c repeated 64 times
		 * and text is the data being protected
		 */
		/* start out by storing key in pads */
		memset( k_ipad, 0, sizeof k_ipad);
		memset( k_opad, 0, sizeof k_opad);
		memcpy( k_ipad, key, key_len);
		memcpy( k_opad, key, key_len);

		/* XOR key with ipad and opad values */
		for (i=0; i<64; i++)
		{
			k_ipad[i] ^= 0x36;
			k_opad[i] ^= 0x5c;
		}

		/*
		 * perform inner MD5
		 */
		SHA1Reset(&context);					/* init context for 1st pass */
		SHA1Input(&context, k_ipad, 64);		/* start with inner pad */
		SHA1Input(&context, text, text_len);	/* then text of datagram */
		if ( ! SHA1Result(&context) )			/* finish up 1st pass */
		{
			return -1;
		}

		/*
		 * perform outer MD5
		 */
		SHA1Reset(&context);					/* init context for 2nd pass */
		SHA1Input(&context, k_opad, 64);		/* start with outer pad */
		SHA1Input(&context, digest, 16);		/* then results of 1st hash */
		if ( ! SHA1Result(&context) )			/* finish up 2nd pass */
		{
			return -1;
		}

		*(((int*)digest) + 0) = context.Message_Digest[0];
		*(((int*)digest) + 1) = context.Message_Digest[1];
		*(((int*)digest) + 2) = context.Message_Digest[2];
		*(((int*)digest) + 3) = context.Message_Digest[3];
		*(((int*)digest) + 4) = context.Message_Digest[4];
	}

	return hash_size;
}


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
int hmac_md5(	unsigned char*	text,
				int				text_len,
				unsigned char*	key,
				int				key_len,
				unsigned char*	digest,
				int				digest_len )
{
	const int hash_size = 16;

	MD5_CTX context;

	int i = 0;
	unsigned char tk[16];
	unsigned char k_ipad[65];	/* inner padding key XORd with ipad */
	unsigned char k_opad[65];	/* outer padding key XORd with opad */

	if ( digest_len >= hash_size )
	{
		/* if key is longer than 64 bytes reset it to key=MD5(key) */
		if ( key_len > 64 )
		{
			MD5_CTX      tctx;

			MD5Init(&tctx);
			MD5Update(&tctx, key, key_len);
			MD5Final(tk, &tctx);

			key = tk;
			key_len = hash_size;
		}

		/*
		 * the HMAC_MD5 transform looks like:
		 *
		 * MD5(K XOR opad, MD5(K XOR ipad, text))
		 *
		 * where K is an n byte key
		 * ipad is the byte 0x36 repeated 64 times
		 * opad is the byte 0x5c repeated 64 times
		 * and text is the data being protected
		 */
		/* start out by storing key in pads */
		memset( k_ipad, 0, sizeof k_ipad);
		memset( k_opad, 0, sizeof k_opad);
		memcpy( k_ipad, key, key_len);
		memcpy( k_opad, key, key_len);

		/* XOR key with ipad and opad values */
		for (i=0; i<64; i++)
		{
			k_ipad[i] ^= 0x36;
			k_opad[i] ^= 0x5c;
		}

		/*
		 * perform inner MD5
		 */
		MD5Init(&context);                   /* init context for 1st pass */
		MD5Update(&context, k_ipad, 64);     /* start with inner pad */
		MD5Update(&context, text, text_len); /* then text of datagram */
		MD5Final(digest, &context);          /* finish up 1st pass */

		/*
		 * perform outer MD5
		 */
		MD5Init(&context);                   /* init context for 2nd pass */
		MD5Update(&context, k_opad, 64);     /* start with outer pad */
		MD5Update(&context, digest, 16);     /* then results of 1st hash */
		MD5Final(digest, &context);          /* finish up 2nd pass */
	}
	
	return hash_size;
}

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
int hmac(	unsigned char*		text,
			int					text_len,
			unsigned char*		key,
			int					key_len,
			unsigned char*		digest,
			int					digest_len,
			hmac_hash_routine	*hash )
{
	int i = 0;
	int hash_size = 0;
	unsigned char *tk = 0;
	unsigned char *hash_inner = 0;
	unsigned char *k_itext = 0;
	unsigned char *k_otext = 0;
	unsigned char k_ipad[65]; /* inner padding key XORd with ipad */
	unsigned char k_opad[65]; /* outer padding key XORd with opad */

	const int block_size = 64;

	/* get digest length, assume digest is in fixed length */
	hash_size = (*hash)((unsigned char*)" ", 1, 0, 0);

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if ( key_len > block_size )
	{
		/* allocate memory for secret key */
		tk = malloc(hash_size);
		if ( NULL == tk )
			return -1;

		if ( hash_size == (*hash)(key, key_len, tk, hash_size) )
		{
			key = tk;
			key_len = hash_size;
		}
		else
		{
			goto _END;
		}
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * hash(K XOR opad, hash(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	memset(k_ipad, 0, sizeof(k_ipad));
	memcpy(k_ipad, key, key_len);
	memset(k_opad, 0, sizeof(k_opad));
	memcpy(k_opad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i=0; i<64; i++)
	{
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner MD5
	 */
	hash_inner = malloc(hash_size);
	k_itext = malloc(block_size + text_len);
	if ( (NULL == hash_inner) || (NULL == k_itext) )
	{
		hash_size = -1;
		goto _END;
	};
	memcpy(k_itext, k_ipad, block_size);
	memcpy(k_itext + block_size, text, text_len);
	(*hash)(k_itext, text_len + block_size, hash_inner, hash_size);

	/*
	 * perform outer MD5
	 */
	k_otext = (unsigned char*)malloc(block_size + hash_size);
	if ( NULL == k_otext )
	{
		hash_size = -1;
		goto _END;
	}
	memcpy(k_otext, k_opad, block_size);
	memcpy(k_otext + block_size, hash_inner, hash_size);
	hash_size = (*hash)(k_otext, block_size + hash_size, digest, digest_len);

_END:
	if ( tk )
		free(tk);

	if ( k_itext )
		free(k_itext);

	if ( k_otext )
		free(k_otext);

	if ( hash_inner )
		free(hash_inner);

	return hash_size;
}
