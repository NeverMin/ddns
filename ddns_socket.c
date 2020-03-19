/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_socket.c 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-02-24.
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
 *  Provide socket support, resolve compatibility issue across platforms.
 */

#include "ddns_socket.h"
#include <string.h>		/* memcpy */
#include "ddns.h"

/*
 *	Initialize socket environment, must be called before calling any of the
 *	socket API.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_socket_init()
{
	int err_code = 0;

#if DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2

	WSADATA wsaData;
#if DDNS_SOCKET_WINSOCK_1
	const WORD wVersionRequested = MAKEWORD(1, 1);
#elif DDNS_SOCKET_WINSOCK_2
	const WORD wVersionRequested = MAKEWORD(2, 0);
#else
#	error "winsock version error!!!"
#endif

	err_code = WSAStartup(wVersionRequested, &wsaData);
	if ( (0 == err_code) && (wVersionRequested != wsaData.wVersion) )
	{
		err_code = -1;
	}
#endif

	return err_code;
}


/*
 *	Uninitialize socket environment.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_socket_uninit()
{
#if DDNS_SOCKET_UNIX
	return 0;
#elif DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
	return WSACleanup();
#else
#	error "socket is not supported on target platform!!!"
#endif
}


/*
 *	Get the last error in socket. Equivalent with 'errno' on UNIX platform.
 *
 *	@return error code the the last error in socket.
 */
int ddns_socket_get_errno()
{
#if DDNS_SOCKET_UNIX
	return errno;
#elif DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
	return WSAGetLastError();
#else
#	error "socket is not supported on target platform!!!"
#endif
}


/*
 *	Set the last error in socket API. Equivalent with 'errno' on UNIX platform.
 *
 *	@param[in]		err		: error code to be set.
 */
void ddns_socket_set_errno(int err)
{
#if DDNS_SOCKET_UNIX
	errno = err;
#elif DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
	WSASetLastError(err);
#else
#	error "socket is not supported on target platform!!!"
#endif
}


/**
 *	Create an endpoint for communication.
 *
 *	@param[in]	af			: specifies a communications domain within which
 *							  communication will take place.
 *								- AF_UNIX: UNIX internal protocols
 *								- AF_INET: ARPA Internet protocols
 *	@param[in]	type		: specifies the semantics of communication.
 *								- SOCK_STREAM
 *								- SOCK_DGRAM
 *								- SOCK_RAW
 *	@param		protocol	: specifies a particular protocol to be used with
 *							  the socket.
 *
 *	@return	descriptor of the newly created endpoint. Use ddns_socket_close to
 *			free resources allocated for the communication endpoint.
 *
 *	@note	the available [af] and [type] depends on what kind of platform you
 *			are running at.
 */
ddns_socket ddns_socket_create(int af, int type, int protocol)
{
	return socket(af, type, protocol);
}


/*
 *	Free resources allocated for a communication endpoint.
 *
 *	@param[in]	sock		: the socket to be destroied.
 */
int ddns_socket_close(ddns_socket sock)
{
#if DDNS_SOCKET_UNIX
	return close(sock);
#elif DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
	return closesocket(sock);
#else
#	error "socket is not supported on target platform!!!"
#endif
}


/**
 *	Initiate a connection on a socket.
 *
 *	@param[in]	sock		: the socket to initiate the connection.
 *	@param[in]	addr		: target address.
 *	@param[in]	sz			: size of the target address in bytes.
 *	@param[in]	timeout		: time out in seconds to initiate the connection.
 *	@param[in]	stop_wait	: pointer to the exit signal, will stop further
 *							  process when it's set to non-zero.
 *
 *	@return Upon successful completion, a value of 0 is returned.  Otherwise, a
 *			value of -1 is returned, use ddns_socket_get_errno to get the error
 *			code.
 *
 *	@note	the socket will be set in non-blocking mode after returned.
 */
int ddns_socket_connect(
	ddns_socket			sock,
	struct sockaddr	*	addr,
	socklen_t			sz,
	int					timeout,
	int				*	stop_wait
	)
{
	int result = -1;

	result = ddns_socket_set_blocking(sock, 0);

	if ( -1 != result )
	{
		result = connect(sock, addr, sz);
#if DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
		if ( -1 == result && EWOULDBLOCK == ddns_socket_get_errno() )
#else
		if ( -1 == result && EINPROGRESS == ddns_socket_get_errno() )
#endif
		{
			int cnt = 0;
			for ( ; cnt < timeout; ++cnt )
			{
				fd_set fd;
				struct timeval tv = { 1, 0 };
				
				if ( (NULL != stop_wait) && (0 != (*stop_wait)) )
				{
					result = -1;
					break;
				}

				FD_ZERO(&fd);
				FD_SET(sock, &fd);

				result = select(sock + 1, 0, &fd, 0, &tv);
				if ( 1 == result )
				{
					/* connected */
					result = 0;
					break;
				}
				else if ( 0 == result )
				{
					/* not connected yet, wait... */
					result = -1;
				}
				else
				{
					/* error */
					break;
				}
			}
		}
	}

	/* test if server has actively refused the connection. */
	if ( 0 == result )
	{
		int error = 0;
		socklen_t len = sizeof(error);
#if DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
		result = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&error, &len);
#elif DDNS_SOCKET_UNIX
		result = getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len);
#else
#		error "socket is not supported on target platform!!!"
#endif
		if ( (0 != result) || (0 != error) )
		{
			result = -1;
		}
	}

	return result;
}


/**
 *	Create a SOCK_STREAM socket and initiate a TCP connection on it.
 *
 *	@param[in]	address		: address of remote host, ex.: "www.website.com".
 *	@param[in]	port		: the remote port to connect to.
 *	@param[in]	timeout		: time out in seconds to initiate the connection.
 *	@param[in]	stop_wait	: pointer to the exit signal, will stop further
 *							  process when it's set to non-zero.
 *
 *	@return	descriptor of the newly created endpoint, default in non-blocking
 *			mode. Use ddns_socket_close to free resources allocated for the
 *			communication endpoint. If it failed to connect to the remote host,
 *			[DDNS_INVALID_SOCKET] will be returned.
 */
ddns_socket ddns_socket_create_tcp(
	const char		*	addr,
	unsigned short		port,
	int					timeout,
	int				*	stop_wait
	)
{
	char				**	in_addr	= NULL;
	ddns_socket				sock	= DDNS_INVALID_SOCKET;
	struct hostent		*	host	= NULL;
	struct sockaddr_in		svr_ip;

	/* resolve host name to ip addresses. */
	host = (struct hostent*)gethostbyname(addr);
	if ( (NULL != host) && (NULL != host->h_addr_list) )
	{
		/* try each ip address of the DDNS server. */
		for ( in_addr = host->h_addr_list; *in_addr; ++in_addr )
		{
			/* check exit signal */
			if ( (NULL != stop_wait) && (0 != (*stop_wait)) )
			{
				break;
			}

			/* construct server address */
			svr_ip.sin_family	= AF_INET;
			svr_ip.sin_port		= htons(port);
			memcpy(&(svr_ip.sin_addr), *in_addr, (int)host->h_length);

			sock = ddns_socket_create(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if ( DDNS_INVALID_SOCKET == sock )
			{
				/* create socket failed, stop further process. */
				break;
			}
			else
			{
				/* succesfully created a socket */
				int result = -1;

				/* connect to server */
				result = ddns_socket_connect(	sock,
												(struct sockaddr*)&svr_ip,
												sizeof(svr_ip),
												timeout,
												stop_wait
												);
				if ( 0 == result )
				{
					/* connected */
					break;
				}
				else
				{
					ddns_socket_close(sock);
					sock = DDNS_INVALID_SOCKET;
				}
			}
		}
	}
	else
	{
		ddns_socket_set_errno(ENETUNREACH);
	}

	return sock;
}


/**
 *	Get blocking mode of a socket.
 *
 *	@param[in]	sock		: the socket to be set.
 *
 *	@return	If it's a blocking socket, non-zero will be returned, otherwise	0
 *			will be returned.
 */
int ddns_socket_get_blocking(
	ddns_socket			sock
	)
{
	int blocking = 1;

#if DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2

	DDNS_UNUSED(sock);

#elif DDNS_SOCKET_UNIX

	int flags = fcntl(sock, F_GETFL);
	if ( 0 == (flags & O_NONBLOCK) )
	{
		blocking = 0;
	}

#else
#	error "socket is not supported on target platform!!!"
#endif

	return blocking;
}


/**
 *	Set blocking property on a socket.
 *
 *	@param[in]	sock		: the socket to be set.
 *	@param[in]	blocking	: blocking mode.
 *								- 0			: set as non-blocking socket
 *								- non-zero	: set as blocking socket
 *
 *	@return Upon successful completion, a value of 0 is returned.  Otherwise, a
 *			value of -1 is returned, use ddns_socket_get_errno to get the error
 *			code.
 */
int ddns_socket_set_blocking(
	ddns_socket			sock,
	int					blocking
	)
{
	int result = -1;

#if DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2

	unsigned long blocking_mode = (0 == blocking) ? 1 : 0;
	result = ioctlsocket(sock, FIONBIO, &blocking_mode);

#elif DDNS_SOCKET_UNIX

	int flags = fcntl(sock, F_GETFL);
	if ( -1 != flags )
	{
		if ( 0 == blocking )
		{
			flags |= O_NONBLOCK;
		}
		else
		{
			flags &= (~O_NONBLOCK);
		}

		result = fcntl(sock, F_SETFL, flags);
	}

#else
#	error "socket is not supported on target platform!!!"
#endif

	return result;
}
