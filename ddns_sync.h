/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_sync.h 292 2013-03-07 08:03:37Z kuang $
 *
 *	This file is part of 'ddns', Created by karl on 2009-02-08.
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
 *  Provide synchronization support across threads.
 */

#ifndef _INC_DDNS_SYNC
#define _INC_DDNS_SYNC

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif

#if defined(HAVE_PTHREAD_H) && HAVE_PTHREAD_H		/* With automake support */
#	define DDNS_SYNC_UNIX			1
#	define DDNS_SYNC_WINDOWS		0
#elif defined(_MSC_VER) || defined(__MINGW32__)		/* Windows: MSVC or MinGW */
#	define DDNS_SYNC_UNIX			0
#	define DDNS_SYNC_WINDOWS		1
#else												/* Unix like platform */
#	define DDNS_SYNC_UNIX			1
#	define DDNS_SYNC_WINDOWS		0
#endif

#if DDNS_SYNC_UNIX
#	include <pthread.h>
#elif DDNS_SYNC_WINDOWS
#	include "ddns_winver.h"
#else
#	error "ddns_sync: target platform is not supported.\n"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct ddns_sync_object
{
#if DDNS_SYNC_UNIX
	pthread_mutex_t		native_sync;
#elif DDNS_SYNC_WINDOWS
	/* use CRITICAL_SECTION on Microsoft Windows */
	CRITICAL_SECTION	native_sync;
#endif
};

/**
 *	Initialize a synchronize object
 *
 *	@param sync_object	: synchronize object to be initialized.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_init(
	struct ddns_sync_object * sync_object
	);


/**
 *	Try to lock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be locked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_trylock(
	struct ddns_sync_object * sync_object
	);


/**
 *	Lock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be locked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_lock(
	struct ddns_sync_object * sync_object
	);


/**
 *	Unlock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be unlocked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_unlock(
	struct ddns_sync_object * sync_object
	);


/**
 *	Free resources allocated for a synchronize object.
 *
 *	@param sync_object	: synchronize object to be freed.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_destroy(
	struct ddns_sync_object * sync_object
	);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* _INC_DDNS_SYNC */
