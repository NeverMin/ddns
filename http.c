/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: http.c 281 2012-10-30 06:00:32Z kuang $
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

#include "ddns.h"
#include "http.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "ddns_string.h"

#ifdef TIME_WITH_SYS_TIME
#	include <sys/time.h>
#	include <time.h>
#elif defined(HAVE_SYS_TIME_H)
#	include <sys/time.h>
#else
#	include <time.h>
#endif

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
#	include <tchar.h>
#	include <wininet.h>
#else
#	include "ddns_socket.h"
#endif

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
#	include <openssl/ssl.h>
#	include <openssl/rand.h>
#endif

#if defined(_MSC_VER)
#	if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
#		pragma comment(lib, "wininet.lib")
#	endif
#	if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
#		pragma comment(lib, "libeay32.lib")
#		pragma comment(lib, "ssleay32.lib")
#	endif
#endif

/*============================================================================*
 *	Local Macros & Global Variables
 *============================================================================*/

#define HTTP_USER_AGENT "crystal-http/1.0 (http://blog.a1983.com.cn)"
#ifndef HTTP_VERSION
#define HTTP_VERSION	"HTTP/1.1"
#endif

/**
 *	Function result value definition.
 *
 *		RESULT_SUCCESS : successful completion.
 *		RESULT_FAILURE : failure
 */
static const int RESULT_FAILURE = 0;
static const int RESULT_SUCCESS = 1;

/*============================================================================*
 *	Declaration of Local Types & Functions
 *============================================================================*/

/**
 *	HTTP header object.
 */
#if (!defined(HTTP_SUPPORT_SSL_WININET)) ||(0 == HTTP_SUPPORT_SSL_WININET)
struct http_header
{
	char							name[32];	/* name of the header         */
	char						*	value;		/* value of the header        */
	struct http_header			*	next;		/* pointer to the next header */
};
#endif


/**
 *	HTTP connection object.
 */
struct http_connection
{
	int								timeout;
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
	int								use_ssl;
	HINTERNET						handle_connect;
	HINTERNET						handle_resource;
#else
	ddns_socket						socket;		/* the socket to communicate */
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	SSL							*	ssl;		/* SSL layer over socket     */
#endif
#endif
};


/**
 *	HTTP request object.
 */
struct http_request
{
	enum http_request_type			method;		/* request method          */
	struct http_connection			connection;	/* HTTP connection         */
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	SSL_CTX						*	ssl_ctx;	/* SSL context, for HTTPS  */
#elif defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
	HINTERNET						handle_open;
	HANDLE							handle_event;
	DWORD							error_code;
#endif
	char							server[64];	/* name of the HTTP server */
	unsigned short					port;		/* TCP port of HTTP server */
	char							path[1024];	/* path of the resource    */
	int								max_redirection;/* maximum redirection count */
#if (!defined(HTTP_SUPPORT_SSL_WININET)) ||(0 == HTTP_SUPPORT_SSL_WININET)
	struct http_header			*	request_hdr;	/* request headers     */
	struct http_header			*	response_hdr;	/* response headers    */
#endif
};


/**
 *	statsu code when parsing a uri.
 */
enum http_uri_parser_status
{
	http_uri_error,					/* invalid uri   */
	http_uri_domain,				/* domain name   */
	http_uri_port,					/* port number   */
	http_uri_path					/* resource path */
};

/**
 *	Get method name of a HTTP request.
 *
 *	@param[in]	request	: the HTTP request.
 *
 *	@return		Method name of the HTTP request. If invalid method is set with
 *				the HTTP request, "GET" will be returned.
 */
static const char * http_get_method_name(
	struct http_request	*	request
	);


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Send HTTP request header to server. For all HTTP requests, it will provide a
 *	default value for the following request headers if it isn't set.
 *
 *		Host			= < server name of the request >
 *		Connection		= "close"
 *		User-Agent		= "ddns"
 *
 *	@param[in]	request	: the HTTP request.
 *
 *	@return		Return non-zero on success, otherwise return 0.
 */
static int http_send_request_header(
	struct http_request		*	request
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Destroy a single request header & free resources allocated for it.
 *
 *	@param[in]	header	: the request header to be destroyed.
 *
 *	@return		Return the next request header in the chain.
 */
static struct http_header * http_free_request_header(
	struct http_header		*	header
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


/**
 *	Free resource allocated for a connection.
 *
 *	@param[in]		connection		: pointer to the connection to be freed.
 */
static void http_free_connection(struct http_connection * connection);


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Send data through a HTTP connection.
 *
 *	@param[in]	connection	: the HTTP connection.
 *	@param[in]	buffer		: the data to be sent over HTTP connection.
 *	@param[in]	size		: size of the data to be sent in bytes.
 *
 *	@return		Upon successful completion, the number of bytes which were sent
 *				is returned.  Otherwise, -1 is returned and a global variable
 *				is set to indicate the error. Use [ddns_socket_get_errno] to
 *				retrieve the error code.
 */
static int http_send(
	struct http_connection	*	connection,
	const char				*	buffer,
	int							size
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Read data from HTTP connection.
 *
 *	@param[in]	connection	: the HTTP connection to read data from.
 *	@param[out]	buffer		: buffer to store the received data.
 *	@param[in]	size		: size of the output buffer in bytes.
 *
 *	@return		Upon successful completion, the number of bytes which were read
 *				is returned.  Otherwise, -1 is returned and a global variable
 *				is set to indicate the error. Use [ddns_socket_get_errno] to
 *				retrieve the error code.
 */
static int http_read(
	struct http_connection	*	connection,
	char					*	buffer,
	int							size
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Read HTTP response headers.
 *
 *	@param[in]	request		: the HTTP request to be processed.
 *
 *	@return		Upon successful completion, the HTTP status code is returned.
 *				Otherwise -1 will be returned.
 */
static int http_read_headers(
	struct http_request		*	request
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


/**
 *	Read HTTP response body.
 *
 *	@param[in]	request		: the HTTP request to be processed.
 *
 *	@return		Upon successful completion, the size of HTTP response body is
 *				returned. Otherwise -1 will be returned.
 */
static int http_read_body(
	struct http_request		*	request,
	http_callback				callback,
	void					*	param
	);


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Append a HTTP header to a header list.
 *
 *	@param[out]	list	: the HTTP request list.
 *	@param[in]	name	: name of the request header.
 *	@param[in]	value	: value of the request header.
 *	@param[in]	force	: if a request header with the same name already exists,
 *						- 0			: ignore the new value
 *						- non-zero	: overwrite existing request header
 *
 *	@return		Return non-zero on success, otherwise 0 will be returned.
 */
static int http_append_header(
	struct http_header	**	list,
	const char			*	name,
	const char			*	value,
	int						force
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Get value of a HTTP header in a header list.
 *
 *	@param[in]	list	: the HTTP header list.
 *	@param[in]	name	: name of the HTTP header to be retrieved.
 *
 *	@return		Value of the HTTP header. If no match found in the header list,
 *				empty string will be returned.
 *
 *	@note		Do *NOT* free the returned pointer, the memory is maintained by
 *				the header list.
 */
static const char * http_get_header(
	struct http_header	*	list,
	const char			*	name
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
/**
 *	Initialize PRNG for OpenSSL.
 *
 *	@return	It returns non-zero if PNRG is fully initialized, otherwise, 0 will
 *			be returned.
 */
static int http_RAND();
#endif	/* HTTP_SUPPORT_SSL_OPENSSL */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Replace the connection of a request from another existing request.
 *
 *	@param[out]		dst		: the destination request.
 *	@param[in]		src		: the source request.
 */
static void http_replace_connection(
	struct http_request * dst,
	struct http_request * src
	);
#endif	/* ! HTTP_SUPPORT_SSL_WININET */

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
/**
 *	WinInet status callback function.
 *
 *	@param[in]	hInternet	: Handle for which the callback function is being called.
 *	@param[in]	dwContext	: Application-defined context value associated with hInternet.
 *	@param[in]	dwStatus	: Status code that indicates why the callback function is being called.
 *	@param[in]	lpvStatusInfo: Address of a buffer that contains information pertinent to this call to the callback function.
 *	@param[in]	dwStatusInfoSize	: Size of the lpvStatusInformation buffer.
 */
static VOID CALLBACK http_status_callback(
	HINTERNET		hInternet,
    DWORD			dwContext,
    DWORD			dwStatus,
    LPVOID			lpvStatusInfo,
    DWORD			dwStatusInfoSize
	);
#endif	/* HTTP_SUPPORT_SSL_WININET */


/*============================================================================*
 *	Implementation of Functions
 *============================================================================*/

/**
 *	Connect to HTTP server
 *
 *	@param[in]	request			: the http request.
 *	@param[in]	timeout			: time out to connect to server, in seconds.
 *
 *	@return		Return 0 on success, otherwise an error code is returned.
 */
int http_connect(struct http_request* request)
{
	int							error_code	= 0;
	struct http_connection	*	conn		= NULL;

	if ( NULL == request )
	{
		return -1;
	}

	conn = &(request->connection);

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET

	/**
	 *	Use WinInet to connect to server.
	 */
	conn->handle_connect = InternetConnectA(request->handle_open,
											request->server,
											request->port,
											NULL,
											NULL,
											INTERNET_SERVICE_HTTP,
											0,
											(DWORD)request
											);
	if ( NULL == conn->handle_connect )
	{
		if ( ERROR_IO_PENDING != GetLastError() )
		{
			error_code = GetLastError();
		}
		else
		{
			DWORD wait_result	= 0;
			DWORD timeout_val	= request->connection.timeout * 1000;

			wait_result = WaitForSingleObject(	request->handle_event,
												timeout_val
												);
			switch( wait_result )
			{
			case WAIT_OBJECT_0:
				error_code = request->error_code;
				break;

			case WAIT_TIMEOUT:
				error_code = ETIMEDOUT;
				break;

			default:
				assert(0);
			}
		}
	}

	if ( ERROR_SUCCESS == error_code )
	{
		const char *	method			= NULL;
		DWORD			request_flags	= 0;

		method = http_get_method_name(request);

		request_flags |= INTERNET_FLAG_DONT_CACHE;
		request_flags |= INTERNET_FLAG_NO_AUTH;
		request_flags |= INTERNET_FLAG_NO_COOKIES;
		request_flags |= INTERNET_FLAG_RELOAD;
		if ( 0 != conn->use_ssl )
		{
			request_flags |= INTERNET_FLAG_SECURE;
		}

		conn->handle_resource = HttpOpenRequestA(	conn->handle_connect,
													method,
													request->path,
													HTTP_VERSION,
													NULL,
													NULL,
													request_flags,
													(DWORD)request
													);
		if ( NULL == conn->handle_resource )
		{
			if ( ERROR_IO_PENDING != GetLastError() )
			{
				error_code = GetLastError();
			}
			else
			{
				DWORD wait_result	= 0;
				DWORD timeout_val	= request->connection.timeout * 1000;

				wait_result = WaitForSingleObject(	request->handle_event,
													timeout_val
													);
				switch( wait_result )
				{
				case WAIT_OBJECT_0:
					error_code = request->error_code;
					break;

				case WAIT_TIMEOUT:
					error_code = ETIMEDOUT;
					break;

				default:
					assert(0);
				}
			}
		}
	}

#else

	/**
	 *	Use raw socket + OpenSSL (optional) to connect to server.
	 */
	conn->socket = ddns_socket_create_tcp(	request->server,
											request->port,
											conn->timeout,
											NULL
											);
	/* error detection */
	if ( DDNS_INVALID_SOCKET == conn->socket )
	{
		error_code = ddns_socket_get_errno();
		if ( ENETUNREACH != error_code )
		{
			error_code = ETIMEDOUT;
		}
	}
	else
	{
		int blocking = (0 != conn->timeout) ? 1 : 0;
		if ( -1 == ddns_socket_set_blocking(conn->socket, blocking) )
		{
			error_code = ENOTSOCK;
		}
	}

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL

	if ( (0 == error_code) && (NULL != request->ssl_ctx) )
	{
		request->connection.ssl = SSL_new(request->ssl_ctx);
		if ( NULL == request->connection.ssl )
		{
			error_code = HTTP_ERROR_SSL;
		}
	}
	if ( (0 == error_code) && (NULL != conn->ssl) )
	{
		if ( 0 == SSL_set_fd(conn->ssl, conn->socket) )
		{
			ddns_socket_set_errno(SSL_get_error(conn->ssl, error_code));
			error_code = HTTP_ERROR_SSL;
		}
	}
	if ( (0 == error_code) && (NULL != conn->ssl) )
	{
		fd_set fd;
		struct timeval tv = { 0, 0 };
		int connect_result = 0;

		FD_ZERO(&fd);
		FD_SET(conn->socket, &fd);

		while ( (connect_result = SSL_connect(conn->ssl)) <= 0 )
		{
			fd_set * fd_read	= NULL;
			fd_set * fd_write	= NULL;
			int ssl_error = SSL_get_error(conn->ssl, connect_result);
			if ( 0 == conn->timeout )
			{
				error_code = ssl_error;
				break;
			}

			if ( SSL_ERROR_WANT_READ == ssl_error )
			{
				fd_read = &fd;
			}
			else if ( SSL_ERROR_WANT_WRITE == ssl_error )
			{
				fd_write = &fd;
			}
			else
			{
				error_code = ssl_error;
				break;
			}

			do
			{
				tv.tv_sec = conn->timeout;
				error_code = select(conn->socket + 1,
									NULL,
									fd_read,
									fd_write,
									&tv
									);
#if DDNS_SOCKET_UNIX
			} while ( (-1 == error_code) && (EINTR == ddns_socket_get_errno()) );
#else
			} while (0);
#endif

			if ( 0 == error_code )
			{
				/* timeout */
				error_code = ETIMEDOUT;
				break;
			}
			else if ( 1 == error_code )
			{
				/* ready to read/write: retry */
				error_code = 0;
			}
			else
			{
				/* unknown error */
				error_code = ssl_error;
				assert(0);
				break;
			}
		}
	}
#endif	/* HTTP_SUPPORT_SSL_OPENSSL */

#endif	/* HTTP_SUPPORT_SSL_WININET */

	return error_code;
}

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
int http_set_option(struct http_request * request, int option, int value)
{
	int original_value = 0;
	if (NULL != request)
	{
		switch(option)
		{
		case HTTP_OPTION_REDIRECT:
			original_value = request->max_redirection;
			request->max_redirection = value;
			break;

		default:
			break;
		}
	}

	return original_value;
}

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
	)
{
	struct http_request * request = NULL;

	/* URI must start with "http://" or "https://" */
	if ( (NULL != uri) && (0 == ddns_strncasecmp("http://", uri, 7)) )
	{
		request = malloc(sizeof(*request));
		if ( NULL != request )
		{
			uri += 7 /* "http://" */;
			memset(request, 0, sizeof(*request));

			request->connection.timeout	= timeout;
#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
			request->connection.socket	= DDNS_INVALID_SOCKET;
#endif
		}
	}
	else if ( (NULL != uri) && (0 == ddns_strncasecmp("https://", uri, 8)) )
	{
		request = malloc(sizeof(*request));
		if ( NULL != request )
		{
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
			static int ssl_init = 0;
#endif

			uri += 8 /* "https://" */;
			memset(request, 0, sizeof(*request));

			request->connection.timeout	= timeout;
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
			request->connection.use_ssl = 1;
#else
			request->connection.socket	= DDNS_INVALID_SOCKET;
#endif

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
			if ( 0 == ssl_init )
			{
				ssl_init = 1;
				SSL_load_error_strings();	/* readable error messages */
				SSL_library_init();			/* initialize library */
				http_RAND();				/* initialize PRNG */
			}

			request->ssl_ctx			= SSL_CTX_new(SSLv23_client_method());
#endif
		}
	}

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
	if ( NULL != request )
	{
		request->handle_event = CreateEvent(NULL, FALSE, FALSE, NULL);
		if ( NULL == request->handle_event )
		{
			http_destroy_request(request);
			request = NULL;
		}
	}
	if ( NULL != request )
	{
		DWORD flags = 0;
		if ( 0 != timeout )
		{
			flags |= INTERNET_FLAG_ASYNC;
		}
		request->handle_open = InternetOpenA(	HTTP_USER_AGENT,
												INTERNET_OPEN_TYPE_DIRECT,
												NULL,
												NULL,
												flags
												);
		if ( NULL == request->handle_open )
		{
			http_destroy_request(request);
			request = NULL;
		}
	}
	if ( NULL != request )
	{
		DWORD dwTimeout = request->connection.timeout * 1000;
		BOOL bResult = InternetSetOption(	request->handle_open,
											INTERNET_OPTION_CONNECT_TIMEOUT,
											&dwTimeout,
											sizeof(dwTimeout)
											);
		if ( FALSE == bResult )
		{
			http_destroy_request(request);
			request = NULL;
		}
	}
	if ( NULL != request )
	{
		InternetSetStatusCallback(request->handle_open, &http_status_callback);
	}
#endif

	if ( NULL != request )
	{
		int								domain_len	= 0;
		int								path_len	= 0;
		enum http_uri_parser_status		status		= http_uri_domain;

		request->method = method;

		for ( ; (http_uri_error != status) && ('\0' != (*uri)); ++uri )
		{
			switch (status)
			{
			case http_uri_domain:		/* host name */
				if ( ':' == (*uri) )
				{
					/* end of host name, followed by port number */
					status = http_uri_port;
				}
				else if ( '/' == (*uri) )
				{
					/* end of host name, followed by resource path */
					--uri;
					status = http_uri_path;
				}
				else if ( domain_len + 1 < sizeof(request->server) )
				{
					/* host name */
					request->server[domain_len++] = (*uri);
				}
				else
				{
					/* insufficient buffer */
					status = http_uri_error;
				}
				break;

			case http_uri_port:			/* port number */
				if ( '/' == (*uri) )
				{
					/* end of port number, followed by resource path */
					--uri;
					status = http_uri_path;
				}
				else if ( ('0' <= (*uri)) && ((*uri) <= '9') )
				{
					/* port number */
					request->port *= 10;
					request->port += ((*uri) - '0');
				}
				else
				{
					status = http_uri_error;
				}
				break;

			case http_uri_path:			/* resource path */
				if ( path_len + 1 < sizeof(request->path) )
				{
					/* get resoruce path */
					request->path[path_len++] = *uri;
				}
				else
				{
					/* insufficient buffer */
					status = http_uri_error;
				}
				break;

			default:
				/* should never reach here */
				status = http_uri_error;
				break;
			}
		}
		if ( http_uri_error != status )
		{
			/**
			 *	RFC2616: If the abs_path is not present in the URL, it MUST be
			 *	given as "/" when used as a Request-URI for a resource.
			 */
			if ( 0 == path_len )
			{
				request->path[path_len++] = '/';
			}
		}
		else
		{
			http_destroy_request(request);
			request = NULL;
		}
	}
	
	if ( NULL != request )
	{
		if ( 0 == request->port )
		{
			/* set a default tcp port */
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
			if ( 0 != request->connection.use_ssl )
#elif defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
			if ( NULL != request->ssl_ctx )
#endif
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
			{
				request->port = 443;
			}
			else
#endif
			{
				request->port = 80;
			}
		}
		if ( ('/' != request->path[0]) && ('\0' != request->path[0]) )
		{
			/* resource path must start from "/" */
			http_destroy_request(request);
			request = NULL;
		}
	}

	return request;
}


/**
 *	Free resources allocated for a HTTP request.
 *
 *	@param[in]	request	: the HTTP request to be destroyed.
 */
void http_destroy_request(
	struct http_request	*	request
	)
{
	if ( NULL == request )
	{
		return;
	}

	/* Step 1: free all dynamically allocated HTTP headers */
#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
	while ( NULL != request->request_hdr )
	{
		request->request_hdr = http_free_request_header(request->request_hdr);
	}
	while ( NULL != request->response_hdr )
	{
		request->response_hdr = http_free_request_header(request->response_hdr);
	}
#endif

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	/* Step 2: close SSL tunnel */
	if ( NULL != request->connection.ssl )
	{
		SSL_free(request->connection.ssl);
		request->connection.ssl = NULL;
	}
	if ( NULL != request->ssl_ctx )
	{
		SSL_CTX_free(request->ssl_ctx);
		request->ssl_ctx = NULL;
	}
#endif

	/* Step 3: close TCP connection */
	http_free_connection(&(request->connection));
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
	if ( NULL != request->handle_open )
	{
		InternetCloseHandle(request->handle_open);
		request->handle_open = NULL;
	}
	if ( NULL != request->handle_event )
	{
		CloseHandle(request->handle_event);
		request->handle_event = NULL;
	}
#endif

	/* Step 4: free the request itself */
	free(request);
	request = NULL;
}


/**
 *	Send a http request to server.
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
	)
{
	int result = RESULT_SUCCESS;
	int status = 0;
	int redirection_count = 0;

	/* Step 1: parameter validity check */
	if ( (NULL == request) || ((NULL == request_body) && (0 != request_size)) )
	{
		result = RESULT_FAILURE;
	}

	/* Step 2: force replace "Content-Length" for "POST" request */
	if ( (RESULT_SUCCESS == result) && (http_method_post == request->method) )
	{
		char header_value[16];
		c99_snprintf(header_value, _countof(header_value), "%d", request_size);
		if ( 0 == http_add_header(request, "Content-Length", header_value, 1) )
		{
			result = RESULT_FAILURE;
		}
	}

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET

	if ( RESULT_SUCCESS == result )
	{
		BOOL send_ok = HttpSendRequestA(request->connection.handle_resource,
										NULL,
										0,
										(LPVOID)request_body,
										request_size
										);
		if (FALSE == send_ok)
		{
			if ( ERROR_IO_PENDING != GetLastError() )
			{
				result = RESULT_FAILURE;
			}
			else
			{
				DWORD wait_result	= 0;
				DWORD timeout_val	= request->connection.timeout * 1000;

				wait_result = WaitForSingleObject(	request->handle_event,
													timeout_val
													);
				switch( wait_result )
				{
				case WAIT_OBJECT_0:
					if ( ERROR_SUCCESS != request->error_code )
					{
						if (ERROR_INTERNET_NAME_NOT_RESOLVED == request->error_code)
						{
							ddns_socket_set_errno(ENETUNREACH);
						}
						else
						{
							ddns_socket_set_errno(request->error_code);
						}
						result = RESULT_FAILURE;
					}
					break;

				case WAIT_TIMEOUT:
					ddns_socket_set_errno(ETIMEDOUT);
					result = RESULT_FAILURE;
					break;

				default:
					assert(0);
				}
			}
		}
	}

	if ( RESULT_SUCCESS == result )
	{
		DWORD	status_size	= sizeof(status);
		DWORD	info_level	= HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE;
		BOOL	query_ok	= FALSE;
		query_ok = HttpQueryInfoA(	request->connection.handle_resource,
									info_level,
									(LPVOID)&status,
									&status_size,
									NULL
									);
		if (FALSE == query_ok)
		{
			result = RESULT_FAILURE;
		}
	}

#else

	/* Step 3: send request header to server */
	do
	{
		if ( RESULT_SUCCESS == result )
		{
			if ( 0 == http_send_request_header(request) )
			{
				result = RESULT_FAILURE;
			}
		}
	
		/* Step 4: send request body to server */
		if ( (RESULT_SUCCESS == result) && (http_method_post == request->method) )
		{
			if ( -1 == http_send(&(request->connection), request_body, request_size) )
			{
				result = RESULT_FAILURE;
			}
		}

		/* Step 5: wait the response from server */
		if (RESULT_SUCCESS == result)
		{
			status = http_read_headers(request);
			if (HTTP_STATUS_PERMANENT_REDIRECT == status || HTTP_STATUS_TEMPORARY_REDIRECT == status)
			{
				const char * redirect_to = http_get_header(request->response_hdr, "Location");
				struct http_request * new_request = http_create_request(request->method, redirect_to, request->connection.timeout);
				if (NULL != new_request && 0 == http_connect(new_request))
				{
					http_replace_connection(request, new_request);
					http_destroy_request(new_request);
					++redirection_count;

					http_add_header(request, "Host", request->server, 1);
					while(NULL != request->response_hdr)
					{
						request->response_hdr = http_free_request_header(request->response_hdr);
					}
				}
				else
				{
					result = RESULT_FAILURE;
				}
			}
			else
			{
				break;
			}
		}

	} while ((RESULT_SUCCESS == result) && (redirection_count < request->max_redirection));

#endif	/* HTTP_SUPPORT_SSL_WININET */

	return status;
}


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
	struct http_request	*	request,
	http_callback			callback,
	void				*	param)
{
	int result = RESULT_SUCCESS;

	if ( -1 == http_read_body(request, callback, param) )
	{
		result = RESULT_FAILURE;
	}

	return result;
}


/**
 *	Add a request header to the request.
 *
 *	@param[out]	request	: the HTTP request to be modified.
 *	@param[in]	name	: name of the request header.
 *	@param[in]	value	: value of the request header.
 *	@param[in]	force	: if a request header with the same name already exists,
 *						- 0			: ignore the new value
 *						- non-zero	: overwrite existing request header
 *
 *	@return		Return non-zero on success, otherwise 0 will be returned.
 */
int http_add_header(
	struct http_request	*	request,
	const char			*	name,
	const char			*	value,
	int						force
	)
{
	int result = RESULT_SUCCESS;

	/* Step 1: parameter validity check */
	if ( RESULT_SUCCESS == result )
	{
		if ( (NULL == request) || (NULL == name) || (NULL == value) )
		{
			result = RESULT_FAILURE;
		}
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
		else if ( NULL == request->connection.handle_resource )
		{
			result = RESULT_FAILURE;
		}
#endif
	}

	/* Step 2: append the header to request header list */
	if ( RESULT_SUCCESS == result )
	{
#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
		char request_hdr[1024];
		DWORD dwModifiers = 0;

		if ( 0 == force )
		{
			dwModifiers = HTTP_ADDREQ_FLAG_ADD_IF_NEW;
		}
		else
		{
			dwModifiers = HTTP_ADDREQ_FLAG_REPLACE | HTTP_ADDREQ_FLAG_ADD;
		}
		c99_snprintf(request_hdr,	_countof(request_hdr),
									"%s: %s",
									name,
									value
									);
		result = HttpAddRequestHeadersA(request->connection.handle_resource,
										request_hdr,
										(DWORD)-1,
										dwModifiers
										);
#else
		result = http_append_header(&(request->request_hdr),
									name,
									value,
									force );
#endif
	}

	return result;
}


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
	)
{
	int src = 0;
	int dst = 0;
	for ( src = 0; (NULL != buffer) && ('\0' != buffer[src]); ++src )
	{
		if (	(buffer[src] >= '0' && buffer[src] <= '9')
			||	(buffer[src] >= 'a' && buffer[src] <= 'z')
			||	(buffer[src] >= 'A' && buffer[src] <= 'Z') )
		{
			if ( dst < out_size )
			{
				output[dst] = buffer[src];
				++dst;
			}
		}
		else if ( buffer[src] == ' ' )
		{
			if ( dst < out_size )
			{
				output[dst] = '+';
			}
		}
		else
		{
			if ( dst + 3 < out_size )
			{
				output[dst] = '%';
				++dst;
				dst += c99_snprintf(output + dst,
									out_size - dst,
									"%02x",
									(int)buffer[src]);
			}
			else
			{
				dst += 3;
			}
		}
	}

	if ( (NULL != output) && (dst < out_size) )
	{
		output[dst] = '\0';
	}
	else
	{
		if ( (NULL != output) && (out_size > 0) )
		{
			output[out_size - 1] = '\0';
		}

		++dst;	/* to hold the null terminator */
	}

	return dst;
}


/**
 *	Get method name of a HTTP request.
 *
 *	@param[in]	request	: the HTTP request.
 *
 *	@return		Method name of the HTTP request. If invalid method is set with
 *				the HTTP request, "GET" will be returned.
 */
static const char * http_get_method_name(
	struct http_request	*	request
	)
{
	const char * method_name = NULL;

	switch ( request->method )
	{
	case http_method_options:
		method_name = "OPTIONS";
		break;

	case http_method_get:
		method_name = "GET";
		break;

	case http_method_head:
		method_name = "HEAD";
		break;

	case http_method_post:
		method_name = "POST";
		break;

	case http_method_put:
		method_name = "PUT";
		break;

	case http_method_delete:
		method_name = "DELETE";
		break;

	case http_method_trace:
		method_name = "TRACE";
		break;

	case http_method_connect:
		method_name = "CONNECT";
		break;

	default:
		return "GET";
	}

	return method_name;
}


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Send HTTP request header to server. For all HTTP requests, it will provide a
 *	default value for the following request headers if they aren't set:
 *
 *		Host			= < server name of the request >
 *		Connection		= "close"
 *		User-Agent		= "crystal-http"
 *		Accept-Charset	= "*"
 *		Accept-Encoding	= "*"
 *		Accept-Language	= "*"
 *
 *	@param[in]	request		: the HTTP request to be sent.
 *
 *	@return		Return non-zero on success, otherwise return 0.
 */
static int http_send_request_header(
	struct http_request		*	request
	)
{
	int result			= RESULT_SUCCESS;
	int header_length	= 0;

	char request_header[1024];

	/* Step 1: parameter validity check */
	if ( RESULT_SUCCESS == result )
	{
		if ( (NULL == request) || ('\0' == request->server[0]) )
		{
			result = RESULT_FAILURE;
		}

		/* RFC 2616: An empty abs_path is equivalent to an abs_path of "/". */
		if ( '\0' == request->path[0] )
		{
			c99_strncpy(request->path, "/", _countof(request->path));
		}
	}

	/* Step 1: set a default value for some common request headers. */
	if ( RESULT_SUCCESS == result )
	{
		/* We don't care any failure in this section. */

		/**
		 *	RFC 2616: The User-Agent request-header field contains information
		 *	about  the  user  agent  originating  the  request.  This  is  for
		 *	statistical  purposes,  the tracing of  protocol  violations,  and
		 *	automated  recognition of user  agents for  the sake of  tailoring
		 *	responses to avoid particular user agent limitations.  User agents
		 *	SHOULD include this field with requests.
		 *
		 *	Provide a default "User-Agent" here.
		 */
		http_add_header(request, "User-Agent", HTTP_USER_AGENT, 0);

		/**
		 *	RFC 2616: A client MUST include a Host header field in all HTTP/1.1
		 *	request messages.
		 */
		http_add_header(request, "Host", request->server, 0);

		/**
		 *	RFC 2616: The Accept-Charset request-header field can be used to
		 *	indicate what character sets are acceptable for the response.
		 *
		 *	We only support US-ASCII charset by default.
		 */
		http_add_header(request, "Accept-Charset", "US-ASCII", 0);

		/**
		 *	RFC 2616: The Accept-Encoding  request-header  field is similar to
		 *	Accept,  but restricts the content-codings (section 3.5)  that are
		 *	acceptable in the response. If no Accept-Encoding field is present
		 *	in a request, the server MAY assume that the client will accept any
		 *	content coding.
		 */
		http_add_header(request, "Accept-Encoding", "*", 0);

		/**
		 *	RFC 2616: If no Accept-Language header is present in the request,
		 *	the server SHOULD assume that all languages are equally acceptable.
		 */
		http_add_header(request, "Accept-Language", "*", 0);

		/**
		 *	RFC 2616: HTTP/1.1 applications that do not support persistent
		 *	connections MUST include the "close" connection option in every
		 *	message.
		 */
		http_add_header(request, "Connection", "close", 0);
	}

	/* Step 2: Construct HTTP request header */
	if ( RESULT_SUCCESS == result )
	{
		unsigned int			len		= 0;

		/**
		 *	Request-Line
		 *
		 *	TODO: [request->path] should be url-encoded.
		 */
		len = c99_snprintf(	request_header,
							_countof(request_header),
							"%s %s " HTTP_VERSION "\r\n",
							http_get_method_name(request),
							request->path );
		if ( _countof(request_header) > len )
		{
			header_length += len;
		}
		else
		{
			result = RESULT_FAILURE;
		}
	}
	if ( RESULT_SUCCESS == result )
	{
		unsigned int			len		= 0;
		struct http_header	*	header	= NULL;

		/**
		 *	N *	(( general-header
		 *		| request-header
		 *		| entity-header ) CRLF)
		 */
		for ( header = request->request_hdr; NULL != header; header = header->next )
		{
			len = c99_snprintf(&(request_header[header_length]),
									_countof(request_header) - header_length,
									"%s: %s\r\n",
									header->name,
									header->value );
			if ( (_countof(request_header) - header_length) > len )
			{
				header_length += len;
			}
			else
			{
				result = RESULT_FAILURE;
				break;
			}
		}
	}
	if ( RESULT_SUCCESS == result )
	{
		/**
		 *	CRLF
		 */
		if ( (_countof(request_header) - header_length) > 2u )
		{
			c99_strncpy(&(request_header[header_length]),
						"\r\n",
						_countof(request_header) - header_length
				);
			header_length += 2;
		}
		else
		{
			result = RESULT_FAILURE;
		}
	}

	/* Step 3: send HTTP request header to server */
	if ( RESULT_SUCCESS == result )
	{
		if ( header_length != http_send(&(request->connection), request_header, header_length) )
		{
			result = RESULT_FAILURE;
		}
	}

	return result;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Append a HTTP header to a header list.
 *
 *	@param[out]	list	: the HTTP request list.
 *	@param[in]	name	: name of the request header.
 *	@param[in]	value	: value of the request header.
 *	@param[in]	force	: if a request header with the same name already exists,
 *						- 0			: ignore the new value
 *						- non-zero	: overwrite existing request header
 *
 *	@return		Return non-zero on success, otherwise 0 will be returned.
 */
static int http_append_header(
	struct http_header	**	list,
	const char			*	name,
	const char			*	value,
	int						force
	)
{
	int						result	= RESULT_SUCCESS;
	struct http_header	*	header	= NULL;

	/* Step 1: parameter validity check */
	if ( RESULT_SUCCESS == result )
	{
		if ( (NULL == list) || (NULL == name) || (NULL == value) )
		{
			result = RESULT_FAILURE;
		}
	}

	/* Step 2: check the request header existence */
	if ( RESULT_SUCCESS == result )
	{
		struct http_header* prev = NULL;
		for(	header = *list;
				NULL != header;
				prev = header, header = header->next )
		{
			if ( 0 == ddns_strcasecmp(name, header->name) )
			{
				if ( 0 == force )
				{
					/* ignore the new request header */
					result = RESULT_FAILURE;
					header = NULL;
				}
				else
				{
					/* replace the existing request header */
					if ( NULL != prev )
					{
						prev->next = http_free_request_header(header);
						header = NULL;
					}
				}
				break;
			}
		}
	}

	/* Step 3: memory allocation */
	if ( RESULT_SUCCESS == result )
	{
		header = malloc(sizeof(*header));
		if ( NULL == header )
		{
			result = RESULT_FAILURE;
		}

		memset(header, 0, sizeof(*header));
		header->value = malloc(strlen(value) + 1);
		if ( NULL == header->value )
		{
			result = RESULT_FAILURE;
		}
	}

	/* Step 4: insert the new request header to the header chain */
	if ( RESULT_SUCCESS == result )
	{
		c99_strncpy(header->name, name, _countof(header->name));
		strcpy(header->value, value);

		header->next = *list;
		*list = header;
	}

	/* Step 5: clean up */
	if ( RESULT_FAILURE == result )
	{
		if ( NULL != header )
		{
			if ( NULL != header->value )
			{
				free(header->value);
				header->value = NULL;
			}

			free(header);
			header = NULL;
		}
	}

	return result;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Destroy a single request header & free resources allocated for it.
 *
 *	@param[in]	header	: the request header to be destroyed.
 *
 *	@return		Return the next request header in the chain.
 */
static struct http_header * http_free_request_header(struct http_header * header)
{
	struct http_header * next_header = NULL;
	if (NULL != header)
	{
		next_header = header->next;
		if ( NULL != header->value )
		{
			free(header->value);
			header->value = NULL;
		}

		free(header);
		header = NULL;
	}
	
	return next_header;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


/**
 *	Free resource allocated for a connection.
 *
 *	@param[in]		connection		: pointer to the connection to be freed.
 */
static void http_free_connection(struct http_connection * connection)
{
	if (NULL == connection)
	{
		return;
	}

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
	if ( NULL != connection->handle_resource )
	{
		InternetCloseHandle(connection->handle_resource);
		connection->handle_resource = NULL;
	}
	if ( NULL != connection->handle_connect )
	{
		InternetCloseHandle(connection->handle_connect);
		connection->handle_connect = NULL;
	}
#else
	if ( DDNS_INVALID_SOCKET != connection->socket )
	{
		ddns_socket_close(connection->socket);
		connection->socket = DDNS_INVALID_SOCKET;
	}
#endif
}


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Send data through a HTTP connection.
 *
 *	@param[in]	connection	: the HTTP connection.
 *	@param[in]	buffer		: the data to be sent over HTTP connection.
 *	@param[in]	size		: size of the data to be sent in bytes.
 *
 *	@return		Upon successful completion, the number of bytes which were sent
 *				is returned.  Otherwise, -1 is returned and a global variable
 *				is set to indicate the error. Use [ddns_socket_get_errno] to
 *				retrieve the error code.
 */
static int http_send(
	struct http_connection	*	connection,
	const char				*	buffer,
	int							size
	)
{
	int retval	= 0;
	int count	= 1;

	if ( (NULL == connection) || (DDNS_INVALID_SOCKET == connection->socket) )
	{
		ddns_socket_set_errno(ENOTSOCK);
		return -1;
	}

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	if ( NULL == connection->ssl )
#endif
	{
		if ( 0 != connection->timeout )
		{
			fd_set fd;
			struct timeval tv = { 0, 0 };

			FD_ZERO(&fd);
			FD_SET(connection->socket, &fd);

			do
			{
				tv.tv_sec = connection->timeout;
				count = select(connection->socket + 1, NULL, &fd, NULL, &tv);
#if DDNS_SOCKET_UNIX
			} while ( (-1 == count) && (EINTR == ddns_socket_get_errno()) );
#else
			} while (0);
#endif
		}

		if ( 0 == count )
		{
			/* timeout */
			ddns_socket_set_errno(ETIMEDOUT);
			retval = -1;
		}
		else if ( 1 == count )
		{
			/* ready to read from socket */
			retval = send(connection->socket, buffer, size, 0);
		}
		else
		{
			/* unknown error */
			retval = -1;
			assert(0);
		}
	}
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	else
	{
		fd_set fd;
		struct timeval tv = { 0, 0 };

		FD_ZERO(&fd);
		FD_SET(connection->socket, &fd);

		while ( (retval = SSL_write(connection->ssl, buffer, size)) <= 0 )
		{
			fd_set * fd_read	= NULL;
			fd_set * fd_write	= NULL;
			int ssl_error = SSL_get_error(connection->ssl, retval);

			if ( 0 == connection->timeout )
			{
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				break;
			}

			if ( SSL_ERROR_WANT_READ == ssl_error )
			{
				fd_read = &fd;
			}
			else if ( SSL_ERROR_WANT_WRITE == ssl_error )
			{
				fd_write = &fd;
			}
			else
			{
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				break;
			}

			do
			{
				tv.tv_sec = connection->timeout;
				count = select(	connection->socket + 1,
								fd_read,
								fd_write,
								NULL,
								&tv
								);
#if DDNS_SOCKET_UNIX
			} while ( (-1 == count) && (EINTR == ddns_socket_get_errno()) );
#else
			} while (0);
#endif

			if ( 0 == count )
			{
				/* timeout */
				ddns_socket_set_errno(ETIMEDOUT);
				retval = -1;
				break;
			}
			else if ( 1 == count )
			{
				/* ready to read/write, do nothing */
			}
			else
			{
				/* unknown error */
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				assert(0);
				break;
			}
		}
	}
#endif

	return retval;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Read data from HTTP connection.
 *
 *	@param[in]	connection	: the HTTP connection to read data from.
 *	@param[out]	buffer		: buffer to store the received data.
 *	@param[in]	size		: size of the output buffer in bytes.
 *
 *	@return		Upon successful completion, the number of bytes which were read
 *				is returned.  Otherwise, -1 is returned and a global variable
 *				is set to indicate the error. Use [ddns_socket_get_errno] to
 *				retrieve the error code.
 */
static int http_read(
	struct http_connection	*	connection,
	char					*	buffer,
	int							size
	)
{
	int retval	= 0;
	int count	= 1;

	if ( (NULL == connection) || (DDNS_INVALID_SOCKET == connection->socket) )
	{
		ddns_socket_set_errno(ENOTSOCK);
		return -1;
	}

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	if ( NULL == connection->ssl )
#endif
	{
		if ( 0 != connection->timeout )
		{
			fd_set fd;
			struct timeval tv = { 0, 0 };
			
			FD_ZERO(&fd);
			FD_SET(connection->socket, &fd);
			
			do
			{
				tv.tv_sec = connection->timeout;
				count = select(connection->socket + 1, &fd, NULL, NULL, &tv);
#if DDNS_SOCKET_UNIX
			} while ( (-1 == count) && (EINTR == ddns_socket_get_errno()) );
#else
			} while (0);
#endif
		}

		if ( 0 == count )
		{
			/* timeout */
			ddns_socket_get_errno(ETIMEDOUT);
			retval = -1;
		}
		else if ( 1 == count )
		{
			/* ready to read from socket */
			retval = recv(connection->socket, buffer, size, 0);
		}
		else
		{
			/* unknown error */
			assert(0);
			retval = -1;
		}
	}
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	else
	{
		fd_set fd;
		struct timeval tv = { 0, 0 };

		FD_ZERO(&fd);
		FD_SET(connection->socket, &fd);

		while ( (retval = SSL_read(connection->ssl, buffer, size)) <= 0 )
		{
			fd_set * fd_read	= NULL;
			fd_set * fd_write	= NULL;
			int ssl_error = SSL_get_error(connection->ssl, retval);
			if ( 0 == connection->timeout )
			{
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				break;
			}

			if ( SSL_ERROR_WANT_READ == ssl_error )
			{
				fd_read = &fd;
			}
			else if ( SSL_ERROR_WANT_WRITE == ssl_error )
			{
				fd_write = &fd;
			}
			else
			{
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				break;
			}

			do
			{
				tv.tv_sec = connection->timeout;
				count = select(	connection->socket + 1,
								fd_read,
								fd_write,
								NULL,
								&tv
								);
#if DDNS_SOCKET_UNIX
			} while ( (-1 == count) && (EINTR == ddns_socket_get_errno()) );
#else
			} while (0);
#endif	

			if ( 0 == count )
			{
				/* timeout */
				ddns_socket_set_errno(ETIMEDOUT);
				retval = -1;
				break;
			}
			else if ( 1 == count )
			{
				/* ready to read/write, do nothing */
			}
			else
			{
				/* unknown error */
				ddns_socket_set_errno(ssl_error);
				retval = -1;
				assert(0);
				break;
			}
		}
	}
#endif

	return retval;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Read HTTP headers.
 *
 *	@param[in]	request		: the HTTP request to be processed.
 *
 *	@return		Upon successful completion, the HTTP status code is returned.
 *				Otherwise -1 will be returned.
 */
static int http_read_headers(
	struct http_request		*	request
	)
{
	enum read_status
	{
		statusVersion,		/* Status-Line: HTTP-Version */
		statusCode,			/* Status-Line: Status-Code */
		statusReason,		/* Status-Line: Reason-Phrase */
		statusName,			/* Header: Name */
		statusColon,		/* Header: Separator between name and value */
		statusValue,		/* Header: Value */
		statusSeparator,	/* Separator between request headers */
		statusBody			/* Message-Body */
	};

	int					result				= 1;
	int					code				= 0;
	char				chr					= 0;
	enum read_status	status				= statusVersion;
	size_t				header_name_len		= 0;
	size_t				header_value_len	= 0;
	char				header_name[64];
	char				header_value[1024];

	memset(header_name, 0, sizeof(header_name));
	memset(header_value, 0, sizeof(header_value));

	/**
	 * Step 1: Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	 */
	while (	(result = http_read(&(request->connection), &chr, 1)) > 0 )
	{
		/* "HTTP-Version" */
		if ( (statusVersion == status) && (' '!= chr) )
		{
			continue;
		}

		/* "SP" between "HTTP-Version" and "Status-Code" */
		if ( (statusVersion == status) && (' ' == chr) )
		{
			status = statusCode;
			continue;
		}

		/* "Status-Code" */
		if ( (statusCode == status) && (chr >= '0') && (chr <= '9') )
		{
			code = code * 10 + (chr - '0');
			continue;
		}
		else if ( (statusCode == status) && (' ' != chr) )
		{
			assert( (chr >= '0') && (chr <= '9') );
		}

		/* "SP" between "Status-Code" and "Reason-Phrase" */
		if ( (statusCode == status) && (' ' == chr) )
		{
			status = statusReason;
			continue;
		}

		/* "CRLF" between "Status-Line" and "HTTP header"s */
		if ( (statusReason == status) && ('\r' == chr) )
		{
			status = statusSeparator;
			continue;
		}
		if ( (statusSeparator == status) && ('\n' == chr) )
		{
			status = statusName;
			break;
		}
		else if ( (statusSeparator == status) && ('\0' != chr) )
		{
			/* fall back to "Reason-Phrase" if only '\r' is found */
			status = statusReason;
			continue;
		}
	}

	/**
	 *	Step 2: handle "general-header", "response-header" and "entity-header"
	 */
	while ( (result > 0) && (http_read(&(request->connection), &chr, 1) > 0) )
	{
		if ( (':' == chr) && (status == statusName) )
		{
			status = statusColon;
		}
		else if ( (' ' != chr) && (statusColon == status) )
		{
			status = statusValue;
		}
		else if ( ('\r' == chr) && (statusValue == status) )
		{
			status = statusSeparator;
		}
		else if ( ('\n' != chr) && (statusSeparator == status) )
		{
			status = statusName;
			http_append_header(	&(request->response_hdr),
								header_name,
								header_value,
								1 );
			header_name_len = 0;
			header_value_len = 0;
			header_name[header_name_len] = '\0';
			header_value[header_value_len] = '\0';
		}
		else if ( ('\n' == chr) && (statusName == status) )
		{
			assert( 0 == strcmp(header_name, "\r") );
			status = statusBody;
			break;
		}

		if ( statusName == status )
		{
			if ( header_name_len < sizeof(header_name) )
			{
				header_name[header_name_len] = chr;
				header_name[++header_name_len] = '\0';
			}
		}
		else if ( statusValue == status )
		{
			if ( header_value_len < sizeof(header_value) )
			{
				header_value[header_value_len] = chr;
				header_value[++header_value_len] = '\0';
			}
		}
	}

	return code;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


/**
 *	Read HTTP response body.
 *
 *	@param[in]	request		: the HTTP request to be processed.
 *
 *	@return		Upon successful completion, the size of HTTP response body is
 *				returned. Otherwise -1 will be returned.
 */
static int http_read_body(
	struct http_request		*	request,
	http_callback				callback,
	void					*	param
	)
{
	long			content_length	= 0;
#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
	const char	*	encoding		= NULL;
#endif

	/* Step 1: parameter validity check */
	if ( NULL == request )
	{
		return -1;
	}

#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET

	if ( NULL != request->connection.handle_resource )
	{
		char	chr		= 0;
		DWORD	recv_bytes	= 0;
		while ( 1 )
		{
			BOOL result = InternetReadFile(request->connection.handle_resource,
											&chr,
											1,
											&recv_bytes
											);
			if (TRUE == result)
			{
				if (recv_bytes > 0)
				{
					++content_length;
					if ( NULL != callback )
					{
						(*callback)(chr, param);
					}
				}
				else
				{
					break;
				}
			}
			else if (ERROR_IO_PENDING == GetLastError())
			{

				switch(WaitForSingleObject(request->handle_event, request->connection.timeout * 1000))
				{
				case WAIT_OBJECT_0:
					break;
					
				default:
					request->error_code = ETIMEDOUT;
					break;
				}

				if (ERROR_SUCCESS == request->error_code)
				{
					continue;
				}
				else
				{
					break;
				}
			}
			else
			{
				break;
			}
		}
	}

#else	/* if (!defined(HTTP_SUPPORT_SSL_OPENSSL)) || (0 == HTTP_SUPPORT_SSL_OPENSSL) */

	encoding = http_get_header(request->response_hdr, "Transfer-Encoding");
	if ( 0 != strcmp("chunked", encoding) )
	{
		char chr = 0;
		while ( http_read(&(request->connection), &chr, 1) > 0 )
		{
			++content_length;
			if ( NULL != callback )
			{
				(*callback)(chr, param);
			}
		}
	}
	else
	{
		/* Transfer-Encoding: chunked */

		enum read_status
		{
			statusSize,			/* chunk-size */
			statusExtension,	/* chunk-extension */
			statusSeparater,	/* separater - CRLF */
			statusBody,			/* chunked body */
			statusTerminator	/* end of a chunked section */
		};

		char				chr			= 0;
		long	 			chunked_size	= 0;
		long				read_size		= 0;
		enum read_status	status			= statusSize;
		while ( http_read(&(request->connection), &chr, 1) > 0 )
		{
			/* Step 1: read "chunk-size" */
			if ( (statusSize == status) && ('0' <= chr) && (chr <= '9') )
			{
				chunked_size = chunked_size * 16 + (chr - '0');
				continue;
			}
			else if ( (statusSize == status) && ('a' <= chr) && (chr <= 'f') )
			{
				chunked_size = chunked_size * 16 + (chr - 'a') + 10;
				continue;
			}
			else if ( (statusSize == status) && ('A' <= chr) && (chr <= 'F') )
			{
				chunked_size = chunked_size * 16 + (chr - 'A') + 10;
				continue;
			}

			/* Step 2: read the optional "chunk-extension" */
			if ( statusSize == status )
			{
				status = statusExtension;
				/* no 'continue', in case extension does not exist */
			}

			/* Step 3: read "CRLF" between "chunk-extension" and "chunk-data" */
			if ( (statusExtension == status) && ('\r' == chr) )
			{
				status = statusSeparater;
				continue;
			}
			else if ( (statusSeparater == status) && ('\n' == chr) )
			{
				status		= statusBody;
				read_size	= 0;
				continue;
			}
			else if ( statusSeparater == status )
			{
				assert('\n' == chr);
				status = statusExtension;
			}

			/* Step 4: read "chunk-data"  */
			if ( (statusBody == status) && (read_size < chunked_size) )
			{
				if ( NULL != callback )
				{
					(*callback)(chr, param);
				}
				++read_size;
				continue;
			}

			/* Step 5: read "CRLF" and the end of a chunk */
			if ( statusBody == status )
			{
				content_length	+= read_size;
				status			= statusTerminator;
			}
			else if ( (statusTerminator == status) && ('\n' == chr) )
			{
				status			= statusSize;
				chunked_size	= 0;
			}
		}
	}

#endif	/* HTTP_SUPPORT_SSL_WININET */

	return content_length;
}


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Get value of a HTTP header in a header list.
 *
 *	@param[in]	list	: the HTTP header list.
 *	@param[in]	name	: name of the HTTP header to be retrieved.
 *
 *	@return		Value of the HTTP header. If no match found in the header list,
 *				empty string will be returned.
 *
 *	@note		Do *NOT* free the returned pointer, the memory is maintained by
 *				the header list.
 */
static const char * http_get_header(
	struct http_header	*	list,
	const char			*	name
	)
{
	const char * value = "";

	for ( ; NULL != list; list = list->next )
	{
		if ( 0 == ddns_strcasecmp(name, list->name) )
		{
			value = list->value;
			break;
		}
	}

	return value;
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if (!defined(HTTP_SUPPORT_SSL_WININET)) || (0 == HTTP_SUPPORT_SSL_WININET)
/**
 *	Replace the connection of a request from another existing request.
 *
 *	@param[out]		dst		: the destination request.
 *	@param[in]		src		: the source request.
 */
static void http_replace_connection(
	struct http_request * dst,
	struct http_request * src
	)
{
	c99_strncpy(dst->server, src->server, sizeof(dst->server));
	c99_strncpy(dst->path, src->path, sizeof(dst->path));

	dst->connection.timeout = src->connection.timeout;

	http_free_connection(&(dst->connection));
	dst->connection.socket = src->connection.socket;
	src->connection.socket = DDNS_INVALID_SOCKET;

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	if (NULL != dst->connection.ssl)
	{
		SSL_free(dst->connection.ssl);
		dst->connection.ssl = NULL;
	}
	dst->connection.ssl = src->connection.ssl;
	src->connection.ssl = NULL;

	if (NULL != dst->ssl_ctx)
	{
		SSL_CTX_free(dst->ssl_ctx);
		dst->ssl_ctx = NULL;
	}
	dst->ssl_ctx = src->ssl_ctx;
	src->ssl_ctx = NULL;
#endif
}
#endif	/* ! HTTP_SUPPORT_SSL_WININET */


#if defined(HTTP_SUPPORT_SSL_WININET) && HTTP_SUPPORT_SSL_WININET
/**
 *	WinInet status callback function.
 *
 *	@param[in]	hInternet	: Handle for which the callback function is being called.
 *	@param[in]	dwContext	: Application-defined context value associated with hInternet.
 *	@param[in]	dwStatus	: Status code that indicates why the callback function is being called.
 *	@param[in]	lpvStatusInfo: Address of a buffer that contains information pertinent to this call to the callback function.
 *	@param[in]	dwStatusInfoSize	: Size of the lpvStatusInformation buffer.
 */
static VOID CALLBACK http_status_callback(
	HINTERNET		hInternet,
    DWORD			dwContext,
    DWORD			dwStatus,
    LPVOID			lpvStatusInfo,
    DWORD			dwStatusInfoSize
	)
{
	struct http_request * request = (struct http_request*)dwContext;

	DDNS_UNUSED(hInternet);

	switch ( dwStatus )
	{
	case INTERNET_STATUS_CLOSING_CONNECTION:
		break;
	case INTERNET_STATUS_CONNECTED_TO_SERVER:
		break;
	case INTERNET_STATUS_CONNECTING_TO_SERVER:
		break;
	case INTERNET_STATUS_CONNECTION_CLOSED:
		break;
	case INTERNET_STATUS_CTL_RESPONSE_RECEIVED:
		break;
	case INTERNET_STATUS_HANDLE_CLOSING:
		break;
	case INTERNET_STATUS_HANDLE_CREATED:
		break;
	case INTERNET_STATUS_INTERMEDIATE_RESPONSE:
		break;
	case INTERNET_STATUS_NAME_RESOLVED:
		break;
	case INTERNET_STATUS_PREFETCH:
		break;
	case INTERNET_STATUS_RECEIVING_RESPONSE:
		break;
	case INTERNET_STATUS_REDIRECT:
		if ((NULL != request) && (request->max_redirection > 0))
		{
			--(request->max_redirection);
		}
		else
		{
			request->error_code = ERROR_HTTP_REDIRECT_FAILED;
			InternetCloseHandle(hInternet);
		}
		break;
	case INTERNET_STATUS_REQUEST_COMPLETE:
		if (NULL != request)
		{
			LPINTERNET_ASYNC_RESULT result	= NULL;

			assert( sizeof(*result) == dwStatusInfoSize );

			if ( NULL != request->handle_event )
			{
				result = (LPINTERNET_ASYNC_RESULT)lpvStatusInfo;
				if (ERROR_SUCCESS == request->error_code)
				{
					request->error_code = result->dwError;
				}

				SetEvent(request->handle_event);
			}
		}
		break;
	case INTERNET_STATUS_REQUEST_SENT:
		break;
	case INTERNET_STATUS_RESOLVING_NAME:
		break;
	case INTERNET_STATUS_RESPONSE_RECEIVED:
		break;
	case INTERNET_STATUS_SENDING_REQUEST:
		break;
	case INTERNET_STATUS_STATE_CHANGE:
		break;
	default:
		break;
	}
}
#endif	/* HTTP_SUPPORT_SSL_WININET */


#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
/**
 *	Initialize PRNG for OpenSSL.
 *
 *	@return	It returns non-zero if PNRG is fully initialized, otherwise, 0 will
 *			be returned.
 */
static int http_RAND()
{
	/**
	 *	Step 1: seed with current time and current process id.
	 */
	time_t	now		= time(NULL);
#if defined(_MSC_VER) || defined(__MINGW32__)
	DWORD	pid		= GetCurrentProcessId();	/* Windows */
#else
	pid_t	pid		= getpid();					/* POSIX.1-2001 */
#endif

	RAND_seed(&now, sizeof(now));
	RAND_seed(&pid, sizeof(pid));

	/**
	 *	Step 2: seed with stack data
	 */
	if ( 0 == RAND_status() )
	{
		/* RAND: do not initialize it */
		char statck_data[256];

		RAND_seed(statck_data, sizeof(statck_data));
	}

	return RAND_status();
}
#endif	/* HTTP_SUPPORT_SSL_OPENSSL */
