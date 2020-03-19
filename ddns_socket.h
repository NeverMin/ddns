/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: ddns_socket.h 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2008-03-31.
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

#ifndef _INC_DDNS_SOCKET
#define _INC_DDNS_SOCKET

#if defined(HAVE_CONFIG_H)
#	include "config.h"
#	if defined(__CYGWIN__)
#		define DDNS_SOCKET_UNIX			1
#		define DDNS_SOCKET_WINSOCK_1	0
#		define DDNS_SOCKET_WINSOCK_2	0
#	elif defined(HAVE_WINSOCK2_H) && HAVE_WINSOCK2_H
#		define DDNS_SOCKET_UNIX			0
#		define DDNS_SOCKET_WINSOCK_1	0
#		define DDNS_SOCKET_WINSOCK_2	1
#	elif defined(HAVE_WINSOCK_H) && HAVE_WINSOCK_H
#		define DDNS_SOCKET_UNIX			0
#		define DDNS_SOCKET_WINSOCK_1	1
#		define DDNS_SOCKET_WINSOCK_2	0
#	else
#		define DDNS_SOCKET_UNIX			1
#		define DDNS_SOCKET_WINSOCK_1	0
#		define DDNS_SOCKET_WINSOCK_2	0
#	endif
#elif defined(WIN32) || defined(UNDER_CE)
#	include "ddns_winver.h"
#	if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0400
#		define DDNS_SOCKET_UNIX			0
#		define DDNS_SOCKET_WINSOCK_1	0
#		define DDNS_SOCKET_WINSOCK_2	1
#	else
#		define DDNS_SOCKET_UNIX			0
#		define DDNS_SOCKET_WINSOCK_1	1
#		define DDNS_SOCKET_WINSOCK_2	0
#	endif
#else
#		define DDNS_SOCKET_UNIX			1
#		define DDNS_SOCKET_WINSOCK_1	0
#		define DDNS_SOCKET_WINSOCK_2	0
#endif

/*********************************************************************/
/* import winsock lib for msvc                                       */
/*********************************************************************/
#if defined(_MSC_VER) && (DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2)
#	if defined(UNDER_CE)
#		pragma comment(lib, "ws2.lib")
#	else
#		pragma comment(lib, "wsock32.lib")
#	endif
#endif

/*********************************************************************/
/* socket is declared in different header file in different platform */
/*********************************************************************/
#if DDNS_SOCKET_UNIX
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_SYS_TYPES_H) && HAVE_SYS_TYPES_H)
#		include <sys/types.h>	/* 'socket': some old BSD require it */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_SYS_SOCKET_H) && HAVE_SYS_SOCKET_H)
#		include <sys/socket.h>	/* POSIX.1-2001: 'socket' */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_SYS_SELECT_H) && HAVE_SYS_SELECT_H)
#		include <sys/select.h>	/* POSIX.1-2001: 'select', 'fd_set', etc. */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_UNISTD_H) && HAVE_UNISTD_H)
#		include <unistd.h>		/* POSIX.1-2001: 'fcntl', 'close'. */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_FCNTL_H) && HAVE_FCNTL_H)
#		include <fcntl.h>		/* POSIX.1-2001: 'fcntl' */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_NETDB_H) && HAVE_NETDB_H)
#		include <netdb.h>		/* POSIX.1-2001: 'gethostbyname' */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_NETINET_IN_H) && HAVE_NETINET_IN_H)
#		include <netinet/in.h>	/* POSIX.1-2001: 'sockaddr_in' */
#	endif
#	if !defined(HAVE_CONFIG_H) || (defined(HAVE_ERRNO_H) && HAVE_ERRNO_H)
#		include <errno.h>		/* 'errno' */
#	endif
#elif DDNS_SOCKET_WINSOCK_2
#	include <stdlib.h>
#	include "ddns_winver.h"
#	ifdef WIN32_LEAN_AND_MEAN
#		include <winsock2.h>
#	endif
#elif DDNS_SOCKET_WINSOCK_1
#	include <stdlib.h>
#	include "ddns_winver.h"
#	ifdef WIN32_LEAN_AND_MEAN
#		include <winsock.h>
#	endif
#endif

/*********************************************************************/
/* some constant is not declared in winsock.h/winsock2.h             */
/*********************************************************************/

/* EWOULDBLOCK is not defined in winsock */
#if !defined(EWOULDBLOCK) && defined(WSAEWOULDBLOCK)
# define EWOULDBLOCK             WSAEWOULDBLOCK
#elif defined(EWOULDBLOCK) && defined(WSAEWOULDBLOCK) && EWOULDBLOCK != WSAEWOULDBLOCK && defined(_MSC_VER)
# pragma message("Do NOT use EWOULDBLOCK, use WSAEWOULDBLOCK instead.")
#elif !defined(EWOULDBLOCK) && defined(_MSC_VER)
# pragma message("EWOULDBLOCK is not defined!")
#endif

/* EINPROGRESS is not defined in winsock */
#if !defined(EINPROGRESS) && defined(WSAEINPROGRESS)
# define EINPROGRESS             WSAEINPROGRESS
#elif defined(EINPROGRESS) && defined(WSAEINPROGRESS) && EINPROGRESS != WSAEINPROGRESS && defined(_MSC_VER)
# pragma message("Do NOT use EINPROGRESS, use WSAEINPROGRESS instead.")
#elif !defined(EINPROGRESS) && defined(_MSC_VER)
# pragma message("EINPROGRESS is not defined!")
#endif

/* EALREADY is not defined in winsock */
#if !defined(EALREADY) && defined(WSAEALREADY)
# define EALREADY             WSAEALREADY
#elif defined(EALREADY) && defined(WSAEALREADY) && EALREADY != WSAEALREADY && defined(_MSC_VER)
# pragma message("Do NOT use EALREADY, use WSAEALREADY instead.")
#elif !defined(EALREADY) && defined(_MSC_VER)
# pragma message("EALREADY is not defined!")
#endif

/* ENOTSOCK is not defined in winsock */
#if !defined(ENOTSOCK) && defined(WSAENOTSOCK)
# define ENOTSOCK             WSAENOTSOCK
#elif defined(ENOTSOCK) && defined(WSAENOTSOCK) && ENOTSOCK != WSAENOTSOCK && defined(_MSC_VER)
# pragma message("Do NOT use ENOTSOCK, use WSAENOTSOCK instead.")
#elif !defined(ENOTSOCK) && defined(_MSC_VER)
# pragma message("ENOTSOCK is not defined!")
#endif

/* EDESTADDRREQ is not defined in winsock */
#if !defined(EDESTADDRREQ) && defined(WSAEDESTADDRREQ)
# define EDESTADDRREQ             WSAEDESTADDRREQ
#elif defined(EDESTADDRREQ) && defined(WSAEDESTADDRREQ) && EDESTADDRREQ != WSAEDESTADDRREQ && defined(_MSC_VER)
# pragma message("Do NOT use EDESTADDRREQ, use WSAEDESTADDRREQ instead.")
#elif !defined(EDESTADDRREQ) && defined(_MSC_VER)
# pragma message("EDESTADDRREQ is not defined!")
#endif

/* EMSGSIZE is not defined in winsock */
#if !defined(EMSGSIZE) && defined(WSAEMSGSIZE)
# define EMSGSIZE             WSAEMSGSIZE
#elif defined(EMSGSIZE) && defined(WSAEMSGSIZE) && EMSGSIZE != WSAEMSGSIZE && defined(_MSC_VER)
# pragma message("Do NOT use EMSGSIZE, use WSAEMSGSIZE instead.")
#elif !defined(EMSGSIZE) && defined(_MSC_VER)
# pragma message("EMSGSIZE is not defined!")
#endif

/* EPROTOTYPE is not defined in winsock */
#if !defined(EPROTOTYPE) && defined(WSAEPROTOTYPE)
# define EPROTOTYPE             WSAEPROTOTYPE
#elif defined(EPROTOTYPE) && defined(WSAEPROTOTYPE) && EPROTOTYPE != WSAEPROTOTYPE && defined(_MSC_VER)
# pragma message("Do NOT use EPROTOTYPE, use WSAEPROTOTYPE instead.")
#elif !defined(EPROTOTYPE) && defined(_MSC_VER)
# pragma message("EPROTOTYPE is not defined!")
#endif

/* ENOPROTOOPT is not defined in winsock */
#if !defined(ENOPROTOOPT) && defined(WSAENOPROTOOPT)
# define ENOPROTOOPT             WSAENOPROTOOPT
#elif defined(ENOPROTOOPT) && defined(WSAENOPROTOOPT) && ENOPROTOOPT != WSAENOPROTOOPT && defined(_MSC_VER)
# pragma message("Do NOT use ENOPROTOOPT, use WSAENOPROTOOPT instead.")
#elif !defined(ENOPROTOOPT) && defined(_MSC_VER)
# pragma message("ENOPROTOOPT is not defined!")
#endif

/* EPROTONOSUPPORT is not defined in winsock */
#if !defined(EPROTONOSUPPORT) && defined(WSAEPROTONOSUPPORT)
# define EPROTONOSUPPORT             WSAEPROTONOSUPPORT
#elif defined(EPROTONOSUPPORT) && defined(WSAEPROTONOSUPPORT) && EPROTONOSUPPORT != WSAEPROTONOSUPPORT && defined(_MSC_VER)
# pragma message("Do NOT use EPROTONOSUPPORT, use WSAEPROTONOSUPPORT instead.")
#elif !defined(EPROTONOSUPPORT) && defined(_MSC_VER)
# pragma message("EPROTONOSUPPORT is not defined!")
#endif

/* ESOCKTNOSUPPORT is not defined in winsock */
#if !defined(ESOCKTNOSUPPORT) && defined(WSAESOCKTNOSUPPORT)
# define ESOCKTNOSUPPORT             WSAESOCKTNOSUPPORT
#elif defined(ESOCKTNOSUPPORT) && defined(WSAESOCKTNOSUPPORT) && ESOCKTNOSUPPORT != WSAESOCKTNOSUPPORT && defined(_MSC_VER)
# pragma message("Do NOT use ESOCKTNOSUPPORT, use WSAESOCKTNOSUPPORT instead.")
#elif !defined(ESOCKTNOSUPPORT) && defined(_MSC_VER)
# pragma message("ESOCKTNOSUPPORT is not defined!")
#endif

/* EOPNOTSUPP is not defined in winsock */
#if !defined(EOPNOTSUPP) && defined(WSAEOPNOTSUPP)
# define EOPNOTSUPP             WSAEOPNOTSUPP
#elif defined(EOPNOTSUPP) && defined(WSAEOPNOTSUPP) && EOPNOTSUPP != WSAEOPNOTSUPP && defined(_MSC_VER)
# pragma message("Do NOT use EOPNOTSUPP, use WSAEOPNOTSUPP instead.")
#elif !defined(EOPNOTSUPP) && defined(_MSC_VER)
# pragma message("EOPNOTSUPP is not defined!")
#endif

/* EPFNOSUPPORT is not defined in winsock */
#if !defined(EPFNOSUPPORT) && defined(WSAEPFNOSUPPORT)
# define EPFNOSUPPORT             WSAEPFNOSUPPORT
#elif defined(EPFNOSUPPORT) && defined(WSAEPFNOSUPPORT) && EPFNOSUPPORT != WSAEPFNOSUPPORT && defined(_MSC_VER)
# pragma message("Do NOT use EPFNOSUPPORT, use WSAEPFNOSUPPORT instead.")
#elif !defined(EPFNOSUPPORT) && defined(_MSC_VER)
# pragma message("EPFNOSUPPORT is not defined!")
#endif

/* EAFNOSUPPORT is not defined in winsock */
#if !defined(EAFNOSUPPORT) && defined(WSAEAFNOSUPPORT)
# define EAFNOSUPPORT             WSAEAFNOSUPPORT
#elif defined(EAFNOSUPPORT) && defined(WSAEAFNOSUPPORT) && EAFNOSUPPORT != WSAEAFNOSUPPORT && defined(_MSC_VER)
# pragma message("Do NOT use EAFNOSUPPORT, use WSAEAFNOSUPPORT instead.")
#elif !defined(EAFNOSUPPORT) && defined(_MSC_VER)
# pragma message("EAFNOSUPPORT is not defined!")
#endif

/* EADDRINUSE is not defined in winsock */
#if !defined(EADDRINUSE) && defined(WSAEADDRINUSE)
# define EADDRINUSE             WSAEADDRINUSE
#elif defined(EADDRINUSE) && defined(WSAEADDRINUSE) && EADDRINUSE != WSAEADDRINUSE && defined(_MSC_VER)
# pragma message("Do NOT use EADDRINUSE, use WSAEADDRINUSE instead.")
#elif !defined(EADDRINUSE) && defined(_MSC_VER)
# pragma message("EADDRINUSE is not defined!")
#endif

/* EADDRNOTAVAIL is not defined in winsock */
#if !defined(EADDRNOTAVAIL) && defined(WSAEADDRNOTAVAIL)
# define EADDRNOTAVAIL             WSAEADDRNOTAVAIL
#elif defined(EADDRNOTAVAIL) && defined(WSAEADDRNOTAVAIL) && EADDRNOTAVAIL != WSAEADDRNOTAVAIL && defined(_MSC_VER)
# pragma message("Do NOT use EADDRNOTAVAIL, use WSAEADDRNOTAVAIL instead.")
#elif !defined(EADDRNOTAVAIL) && defined(_MSC_VER)
# pragma message("EADDRNOTAVAIL is not defined!")
#endif

/* ENETDOWN is not defined in winsock */
#if !defined(ENETDOWN) && defined(WSAENETDOWN)
# define ENETDOWN             WSAENETDOWN
#elif defined(ENETDOWN) && defined(WSAENETDOWN) && ENETDOWN != WSAENETDOWN && defined(_MSC_VER)
# pragma message("Do NOT use ENETDOWN, use WSAENETDOWN instead.")
#elif !defined(ENETDOWN) && defined(_MSC_VER)
# pragma message("ENETDOWN is not defined!")
#endif

/* ENETUNREACH is not defined in winsock */
#if !defined(ENETUNREACH) && defined(WSAENETUNREACH)
# define ENETUNREACH             WSAENETUNREACH
#elif defined(ENETUNREACH) && defined(WSAENETUNREACH) && ENETUNREACH != WSAENETUNREACH && defined(_MSC_VER)
# pragma message("Do NOT use ENETUNREACH, use WSAENETUNREACH instead.")
#elif !defined(ENETUNREACH) && defined(_MSC_VER)
# pragma message("ENETUNREACH is not defined!")
#endif

/* ENETRESET is not defined in winsock */
#if !defined(ENETRESET) && defined(WSAENETRESET)
# define ENETRESET             WSAENETRESET
#elif defined(ENETRESET) && defined(WSAENETRESET) && ENETRESET != WSAENETRESET && defined(_MSC_VER)
# pragma message("Do NOT use ENETRESET, use WSAENETRESET instead.")
#elif !defined(ENETRESET) && defined(_MSC_VER)
# pragma message("ENETRESET is not defined!")
#endif

/* ECONNABORTED is not defined in winsock */
#if !defined(ECONNABORTED) && defined(WSAECONNABORTED)
# define ECONNABORTED             WSAECONNABORTED
#elif defined(ECONNABORTED) && defined(WSAECONNABORTED) && ECONNABORTED != WSAECONNABORTED && defined(_MSC_VER)
# pragma message("Do NOT use ECONNABORTED, use WSAECONNABORTED instead.")
#elif !defined(ECONNABORTED) && defined(_MSC_VER)
# pragma message("ECONNABORTED is not defined!")
#endif

/* ECONNRESET is not defined in winsock */
#if !defined(ECONNRESET) && defined(WSAECONNRESET)
# define ECONNRESET             WSAECONNRESET
#elif defined(ECONNRESET) && defined(WSAECONNRESET) && ECONNRESET != WSAECONNRESET && defined(_MSC_VER)
# pragma message("Do NOT use ECONNRESET, use WSAECONNRESET instead.")
#elif !defined(ECONNRESET) && defined(_MSC_VER)
# pragma message("ECONNRESET is not defined!")
#endif

/* ENOBUFS is not defined in winsock */
#if !defined(ENOBUFS) && defined(WSAENOBUFS)
# define ENOBUFS             WSAENOBUFS
#elif defined(ENOBUFS) && defined(WSAENOBUFS) && ENOBUFS != WSAENOBUFS && defined(_MSC_VER)
# pragma message("Do NOT use ENOBUFS, use WSAENOBUFS instead.")
#elif !defined(ENOBUFS) && defined(_MSC_VER)
# pragma message("ENOBUFS is not defined!")
#endif

/* EISCONN is not defined in winsock */
#if !defined(EISCONN) && defined(WSAEISCONN)
# define EISCONN             WSAEISCONN
#elif defined(EISCONN) && defined(WSAEISCONN) && EISCONN != WSAEISCONN && defined(_MSC_VER)
# pragma message("Do NOT use EISCONN, use WSAEISCONN instead.")
#elif !defined(EISCONN) && defined(_MSC_VER)
# pragma message("EISCONN is not defined!")
#endif

/* ENOTCONN is not defined in winsock */
#if !defined(ENOTCONN) && defined(WSAENOTCONN)
# define ENOTCONN             WSAENOTCONN
#elif defined(ENOTCONN) && defined(WSAENOTCONN) && ENOTCONN != WSAENOTCONN && defined(_MSC_VER)
# pragma message("Do NOT use ENOTCONN, use WSAENOTCONN instead.")
#elif !defined(ENOTCONN) && defined(_MSC_VER)
# pragma message("ENOTCONN is not defined!")
#endif

/* ESHUTDOWN is not defined in winsock */
#if !defined(ESHUTDOWN) && defined(WSAESHUTDOWN)
# define ESHUTDOWN             WSAESHUTDOWN
#elif defined(ESHUTDOWN) && defined(WSAESHUTDOWN) && ESHUTDOWN != WSAESHUTDOWN && defined(_MSC_VER)
# pragma message("Do NOT use ESHUTDOWN, use WSAESHUTDOWN instead.")
#elif !defined(ESHUTDOWN) && defined(_MSC_VER)
# pragma message("ESHUTDOWN is not defined!")
#endif

/* ETOOMANYREFS is not defined in winsock */
#if !defined(ETOOMANYREFS) && defined(WSAETOOMANYREFS)
# define ETOOMANYREFS             WSAETOOMANYREFS
#elif defined(ETOOMANYREFS) && defined(WSAETOOMANYREFS) && ETOOMANYREFS != WSAETOOMANYREFS && defined(_MSC_VER)
# pragma message("Do NOT use ETOOMANYREFS, use WSAETOOMANYREFS instead.")
#elif !defined(ETOOMANYREFS) && defined(_MSC_VER)
# pragma message("ETOOMANYREFS is not defined!")
#endif

/* ETIMEDOUT is not defined in winsock */
#if !defined(ETIMEDOUT) && defined(WSAETIMEDOUT)
# define ETIMEDOUT             WSAETIMEDOUT
#elif defined(ETIMEDOUT) && defined(WSAETIMEDOUT) && ETIMEDOUT != WSAETIMEDOUT && defined(_MSC_VER)
# pragma message("Do NOT use ETIMEDOUT, use WSAETIMEDOUT instead.")
#elif !defined(ETIMEDOUT) && defined(_MSC_VER)
# pragma message("ETIMEDOUT is not defined!")
#endif

/* ECONNREFUSED is not defined in winsock */
#if !defined(ECONNREFUSED) && defined(WSAECONNREFUSED)
# define ECONNREFUSED             WSAECONNREFUSED
#elif defined(ECONNREFUSED) && defined(WSAECONNREFUSED) && ECONNREFUSED != WSAECONNREFUSED && defined(_MSC_VER)
# pragma message("Do NOT use ECONNREFUSED, use WSAECONNREFUSED instead.")
#elif !defined(ECONNREFUSED) && defined(_MSC_VER)
# pragma message("ECONNREFUSED is not defined!")
#endif

/* ELOOP is not defined in winsock */
#if !defined(ELOOP) && defined(WSAELOOP)
# define ELOOP             WSAELOOP
#elif defined(ELOOP) && defined(WSAELOOP) && ELOOP != WSAELOOP && defined(_MSC_VER)
# pragma message("Do NOT use ELOOP, use WSAELOOP instead.")
#elif !defined(ELOOP) && defined(_MSC_VER)
# pragma message("ELOOP is not defined!")
#endif

/* !!! conflict !!! */
/* ENAMETOOLONG is not defined in winsock */
#if !defined(ENAMETOOLONG) && defined(WSAENAMETOOLONG)
# define ENAMETOOLONG             WSAENAMETOOLONG
#elif defined(ENAMETOOLONG) && defined(WSAENAMETOOLONG) && ENAMETOOLONG != WSAENAMETOOLONG && defined(_MSC_VER)
# pragma message("Do NOT use ENAMETOOLONG, use WSAENAMETOOLONG instead.")
#elif !defined(ENAMETOOLONG) && defined(_MSC_VER)
# pragma message("ENAMETOOLONG is not defined!")
#endif

/* EHOSTDOWN is not defined in winsock */
#if !defined(EHOSTDOWN) && defined(WSAEHOSTDOWN)
# define EHOSTDOWN             WSAEHOSTDOWN
#elif defined(EHOSTDOWN) && defined(WSAEHOSTDOWN) && EHOSTDOWN != WSAEHOSTDOWN && defined(_MSC_VER)
# pragma message("Do NOT use EHOSTDOWN, use WSAEHOSTDOWN instead.")
#elif !defined(EHOSTDOWN) && defined(_MSC_VER)
# pragma message("EHOSTDOWN is not defined!")
#endif

/* EHOSTUNREACH is not defined in winsock */
#if !defined(EHOSTUNREACH) && defined(WSAEHOSTUNREACH)
# define EHOSTUNREACH             WSAEHOSTUNREACH
#elif defined(EHOSTUNREACH) && defined(WSAEHOSTUNREACH) && EHOSTUNREACH != WSAEHOSTUNREACH && defined(_MSC_VER)
# pragma message("Do NOT use EHOSTUNREACH, use WSAEHOSTUNREACH instead.")
#elif !defined(EHOSTUNREACH) && defined(_MSC_VER)
# pragma message("EHOSTUNREACH is not defined!")
#endif

/* ENOTEMPTY is not defined in winsock */
#if !defined(ENOTEMPTY) && defined(WSAENOTEMPTY)
# define ENOTEMPTY             WSAENOTEMPTY
#elif defined(ENOTEMPTY) && defined(WSAENOTEMPTY) && ENOTEMPTY != WSAENOTEMPTY && defined(_MSC_VER)
# pragma message("Do NOT use ENOTEMPTY, use WSAENOTEMPTY instead.")
#elif !defined(ENOTEMPTY) && defined(_MSC_VER)
# pragma message("ENOTEMPTY is not defined!")
#endif

/* EPROCLIM is not defined in winsock */
#if !defined(EPROCLIM) && defined(WSAEPROCLIM)
# define EPROCLIM             WSAEPROCLIM
#elif defined(EPROCLIM) && defined(WSAEPROCLIM) && EPROCLIM != WSAEPROCLIM && defined(_MSC_VER)
# pragma message("Do NOT use EPROCLIM, use WSAEPROCLIM instead.")
#elif !defined(EPROCLIM) && defined(_MSC_VER)
# pragma message("EPROCLIM is not defined!")
#endif

/* EUSERS is not defined in winsock */
#if !defined(EUSERS) && defined(WSAEUSERS)
# define EUSERS             WSAEUSERS
#elif defined(EUSERS) && defined(WSAEUSERS) && EUSERS != WSAEUSERS && defined(_MSC_VER)
# pragma message("Do NOT use EUSERS, use WSAEUSERS instead.")
#elif !defined(EUSERS) && defined(_MSC_VER)
# pragma message("EUSERS is not defined!")
#endif

/* EDQUOT is not defined in winsock */
#if !defined(EDQUOT) && defined(WSAEDQUOT)
# define EDQUOT             WSAEDQUOT
#elif defined(EDQUOT) && defined(WSAEDQUOT) && EDQUOT != WSAEDQUOT && defined(_MSC_VER)
# pragma message("Do NOT use EDQUOT, use WSAEDQUOT instead.")
#elif !defined(EDQUOT) && defined(_MSC_VER)
# pragma message("EDQUOT is not defined!")
#endif

/* ESTALE is not defined in winsock */
#if !defined(ESTALE) && defined(WSAESTALE)
# define ESTALE             WSAESTALE
#elif defined(ESTALE) && defined(WSAESTALE) && ESTALE != WSAESTALE && defined(_MSC_VER)
# pragma message("Do NOT use ESTALE, use WSAESTALE instead.")
#elif !defined(ESTALE) && defined(_MSC_VER)
# pragma message("ESTALE is not defined!")
#endif

/* EREMOTE is not defined in winsock */
#if !defined(EREMOTE) && defined(WSAEREMOTE)
# define EREMOTE             WSAEREMOTE
#elif defined(EREMOTE) && defined(WSAEREMOTE) && EREMOTE != WSAEREMOTE && defined(_MSC_VER)
# pragma message("Do NOT use EREMOTE, use WSAEREMOTE instead.")
#elif !defined(EREMOTE) && defined(_MSC_VER)
# pragma message("EREMOTE is not defined!")
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*********************************************************************/
/* some types are not declared in winsock.h/winsock2.h               */
/*********************************************************************/
#if DDNS_SOCKET_UNIX

	typedef int			ddns_socket;

#elif DDNS_SOCKET_WINSOCK_1 || DDNS_SOCKET_WINSOCK_2
	
	typedef int			socklen_t;
	typedef SOCKET		ddns_socket;

#endif
	
#define DDNS_INVALID_SOCKET		((ddns_socket)(-1))


/*
 *	Initialize socket environment, must be called before calling any of the
 *	socket API.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_socket_init();


/*
 *	Uninitialize socket environment.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_socket_uninit();


/*
 *	Get the last error in socket. Equivalent with 'errno' on UNIX platform.
 *
 *	@return error code the the last error in socket.
 */
int ddns_socket_get_errno();


/*
 *	Set the last error in socket API. Equivalent with 'errno' on UNIX platform.
 *
 *	@param[in]	err			: error code to be set.
 */
void ddns_socket_set_errno(int err);


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
ddns_socket ddns_socket_create(int af, int type, int protocol);


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
	);


/*
 *	Free resources allocated for a communication endpoint.
 *
 *	@param[in]	sock		: the socket to be destroyed.
 */
int ddns_socket_close(ddns_socket sock);


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
	);


/**
 *	Get blocking mode of a socket.
 *
 *	@param[in]	sock		: the socket to be set.
 *
 *	@return	If it's a blocking socket, non-zero will be returned, otherwise
 *			0 will be returned.
 */
int ddns_socket_get_blocking(
	ddns_socket			sock
	);


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
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* !defined(_INC_DDNS_SOCKET) */
