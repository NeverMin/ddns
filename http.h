/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: http.h 273 2012-04-23 19:09:33Z kuang $
 *
 *	This file is part of 'ddns', Created by karl on 2009-05-22.
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
 *  Provide HTTP protocol support.
 */

#ifndef _INC_HTTP
#define _INC_HTTP

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
#	define HTTP_SUPPORT_SSL_OPENSSL		0
#	define HTTP_SUPPORT_SSL_WININET		1
#elif defined(HAVE_OPENSSL_SSL_H) && HAVE_OPENSSL_SSL_H
#	define HTTP_SUPPORT_SSL_OPENSSL		1
#	define HTTP_SUPPORT_SSL_WININET		0
#else
#	define HTTP_SUPPORT_SSL_OPENSSL		0
#	define HTTP_SUPPORT_SSL_WININET		0
#endif
#if HTTP_SUPPORT_SSL_OPENSSL || HTTP_SUPPORT_SSL_WININET
#	define HTTP_SUPPORT_SSL		1
#else
#	define HTTP_SUPPORT_SSL		0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_ERROR_SSL		2

#define HTTP_STATUS_PERMANENT_REDIRECT		301
#define HTTP_STATUS_TEMPORARY_REDIRECT		302


/**
 *	Constants for [http_set_option]
 */
#define HTTP_OPTION_REDIRECT	0	/* maximum allowed redirection count */

enum http_request_type
{
	http_method_options,
	http_method_get,
	http_method_head,
	http_method_post,
	http_method_put,
	http_method_delete,
	http_method_trace,
	http_method_connect
};

struct http_request;

typedef void (*http_callback)(char chr, void* param);


/**
 *	Connect to HTTP server
 *
 *	@param[in]	request			: the HTTP request.
 *
 *	@return		Return 0 on success, otherwise an error code is returned.
 */
int http_connect(struct http_request* request);

/**
 *	Set options of the request.
 *
 *	@param[in]	request			: the HTTP request.
 *	@param[in]	option			: type of the option, supported values are:
 *									- HTTP_OPTION_REDIRECT
 *	@param[in]	value			: value for the option.
 *
 *	@return		Return the original value of the request option.
 */
int http_set_option(struct http_request * request, int option, int value);

/**
 *	Send a HTTP request to server.
 *
 *	@param[in]	request			: http request header.
 *	@param[in]	request_body	: http request body.
 *	@param[in]	request_size	: size of request body in bytes.
 *
 *	@return		Return HTTP response status on success, otherwise return 0.
 */
int http_send_request(
	struct http_request		*	request,
	const char				*	request_body,
	int							request_size
	);


/**
 *	Get HTTP response from server.
 *
 *	@param[in]	request		: the HTTP request
 *	@param[in]	callback	: pointer to the callback function, every received
 *							  byte will be passed to it
 *	@param[in]	param		: extra parameter to be passed to callback function
 *
 *	@return		Return non-zero on success, otherwise 0 will be returned.
 */
int http_get_response(
	struct http_request		*	request,
	http_callback				callback,
	void					*	param
);


/**
 *	Create a HTTP request.
 *
 *	@param[in]	method		: HTTP request method
 *	@param[in]	uri			: URI of the requested resource.
 *	@param[in]	timeout		: operation timeout in seconds. If it's 0, default
 *							  timeout of your platform will be used.
 *
 *	@return		Return pointer to the newly created HTTP request, when finished
 *				using it, please use [http_destroy_request] to free it.
 *				If any error occurred, NULL will be returned.
 */
struct http_request * http_create_request(
	enum http_request_type		method,
	const char				*	uri,
	int							timeout
	);


/**
 *	Free resources allocated for a HTTP request.
 *
 *	@param[in]	request	: the HTTP request to be destroyed.
 */
void http_destroy_request(
	struct http_request	*	request
	);


/**
 *	Add a request header to the request.
 *
 *	@param[out]	request	: the HTTP request to be modified.
 *	@param[in]	name	: name of the request header.
 *	@param[in]	value	: value of the request header.
 *	@param[in]	force	: if a request header with the same name already exists,
 *							- 0			: ignore the new value
 *							- non-zero	: overwrite existing request header
 *
 *	@return		Return non-zero on success, otherwise 0 will be returned.
 */
int http_add_header(
	struct http_request	*	request,
	const char			*	name,
	const char			*	value,
	int						force
	);


/**
 *	Do url-encode
 *
 *	@param[in]	buffer	: the string to be escaped.
 *	@param[out]	output	: the buffer to store the escaped string.
 *	@param[in]	out_size: size of the output buffer in bytes.
 *
 *	@return		If the return value is smaller than [out_size], it's the actual
 *				character number wrote into the output buffer, excluding the
 *				null terminator. Otherwise, it's the required buffer size to
 *				hold the escaped string.
 */
int http_urlencode(
	const char	*	buffer,
	char		*	output,
	int				out_size
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* _INC_HTTP */
