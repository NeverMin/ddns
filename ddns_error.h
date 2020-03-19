/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_error.h 289 2013-03-07 04:23:41Z kuang $
 *
 *	This file is part of 'ddns', Created by kuang on 2009-12-20.
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
 *	Define common error code.
 */

#ifndef _INC_DDNS_ERROR
#define _INC_DDNS_ERROR

typedef unsigned long	ddns_error;

#define DDNS_ERROR_BASE					((ddns_error)0x40000000)

#define DDNS_FATAL_ERROR(err)			((ddns_error)((err) | 0x80000000))
#define DDNS_IS_FATAL_ERROR(err)		(0 != ((err) & 0x80000000))

/**
 *	The operation succeeded.
 */
#define DDNS_ERROR_SUCCESS				((ddns_error)0)


/**
 *	Unknown error happened.
 */
#define DDNS_ERROR_UNKNOWN				((ddns_error)(DDNS_ERROR_BASE + 0))


/**
 *	Authentication failed, it's typically because of bad user/password pair.
 */
#define DDNS_ERROR_BADAUTH				((ddns_error)(DDNS_ERROR_BASE + 1))


/**
 *	Connection timeout, it's typically because of bad network.
 */
#define DDNS_ERROR_TIMEOUT				((ddns_error)(DDNS_ERROR_BASE + 2))


/**
 *	Blocked, either the domain name or your DDNS account, please contact your
 *	DDNS service provider for a solution.
 */
#define DDNS_ERROR_BLOCKED				((ddns_error)(DDNS_ERROR_BASE + 3))


/**
 *	Get unexpected return from DDNS server, typically it's because incorrect
 *	DDNS server is specified.
 */
#define DDNS_ERROR_BADSVR				((ddns_error)(DDNS_ERROR_BASE + 4))


/**
 *	IP address isn't changed since last call.
 */
#define DDNS_ERROR_NOCHG				((ddns_error)(DDNS_ERROR_BASE + 5))


/**
 *	The interface isn't implemented yet.
 */
#define DDNS_ERROR_NOTIMPL				((ddns_error)(DDNS_ERROR_BASE + 6))


/**
 *	Incorrect parameters are passed in for a function.
 */
#define DDNS_ERROR_BADARG				((ddns_error)(DDNS_ERROR_BASE + 7))


/**
 *	Insufficient memory to accomplish the operation.
 */
#define DDNS_ERROR_INSUFFICIENT_MEMORY	((ddns_error)(DDNS_ERROR_BASE + 8))


 /**
 *	Insufficient buffer to accomplish the operation.
 */
#define DDNS_ERROR_INSUFFICIENT_BUFFER	((ddns_error)(DDNS_ERROR_BASE + 9))


/**
 *	Invalid URL is specified.
 */
#define DDNS_ERROR_BADURL				((ddns_error)(DDNS_ERROR_BASE + 10))


/**
 *	Connection error, typically network problem.
 */
#define DDNS_ERROR_CONNECTION			((ddns_error)(DDNS_ERROR_BASE + 11))


/**
 *	The DDNS context isn't initialized yet.
 */
#define DDNS_ERROR_UNINIT				((ddns_error)(DDNS_ERROR_BASE + 12))


/**
 *	The DDNS agent is blocked.
*/
#define DDNS_ERROR_BADAGENT				((ddns_error)(DDNS_ERROR_BASE + 13))


/**
 *	Trying to use a paid feature using free account.
 */
#define DDNS_ERROR_PAIDFEATURE			((ddns_error)(DDNS_ERROR_BASE + 14))


/**
 *	The hostname specified does not exist in this user account.
 */
#define DDNS_ERROR_NOHOST				((ddns_error)(DDNS_ERROR_BASE + 15))


/**
 *	The domain specified is not in your account.
 */
#define DDNS_ERROR_NODOMAIN				((ddns_error)(DDNS_ERROR_BASE + 16))


/**
 *	The hostname specified is not a fully-qualified domain name.
 */
#define DDNS_ERROR_BADDOMAIN			((ddns_error)(DDNS_ERROR_BASE + 17))


/**
 *	The DDNS server is down (either a problem or in maintenance).
 */
#define DDNS_ERROR_SVRDOWN				((ddns_error)(DDNS_ERROR_BASE + 18))


/**
 *	Duplicate domain found: domain name already exists.
 */
#define DDNS_ERROR_DUPDOMAIN			((ddns_error)(DDNS_ERROR_BASE + 19))


/**
 *	The domain specified does not exist in the database.
 */
#define DDNS_ERROR_NXDOMAIN				((ddns_error)(DDNS_ERROR_BASE + 20))


/**
 *	Successful, but no record returned.
 */
#define DDNS_ERROR_EMPTY				((ddns_error)(DDNS_ERROR_BASE + 21))


/**
 *	Connection reset by remote host.
 */
#define DDNS_ERROR_CONNRST				((ddns_error)(DDNS_ERROR_BASE + 22))


/**
 *	Connection closed by remote host.
 */
#define DDNS_ERROR_CONNCLOSE			((ddns_error)(DDNS_ERROR_BASE + 23))


/**
 *	Unable to reach remote host, DNS lookup error or net unreachable.
 */
#define DDNS_ERROR_UNREACHABLE			((ddns_error)(DDNS_ERROR_BASE + 24))


/**
 *	Client should connect to an alternative server.
 */
#define DDNS_ERROR_REDIRECT				((ddns_error)(DDNS_ERROR_BASE + 25))


/**
 *	Maximum redirect count has reached.
 */
#define DDNS_ERROR_MAXREDIRECT			((ddns_error)(DDNS_ERROR_BASE + 26))


/**
 *	No available server found.
 */
#define DDNS_ERROR_NO_SERVER			((ddns_error)(DDNS_ERROR_BASE + 27))


/**
 *	SSL support is required.
 */
#define DDNS_ERROR_SSL_REQUIRED			((ddns_error)(DDNS_ERROR_BASE + 28))


/**
 *	Unsupported DDNS protocol.
 */
#define DDNS_ERROR_INVALID_PROTO		((ddns_error)(DDNS_ERROR_BASE + 29))

#endif	/* ! _INC_DDNS_ERROR */
