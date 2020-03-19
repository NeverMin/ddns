/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_sync.c 243 2010-07-13 14:10:07Z karl $
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

#include "ddns_sync.h"

/**
 *	Initialize a synchronize object
 *
 *	@param sync_object	: synchronize object to be initialized.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_init(
	struct ddns_sync_object * sync_object )
{
	int status = 0;

#if DDNS_SYNC_UNIX

	pthread_mutexattr_t ma;
	status = pthread_mutexattr_init(&ma);
#ifndef __CYGWIN__
#ifdef PTHREAD_PRIO_NONE
	if ( 0 == status )
	{
		status = pthread_mutexattr_setprotocol(&ma, PTHREAD_PRIO_NONE);
	}
#endif
#endif
#ifdef PTHREAD_MUTEX_RECURSIVE
	if ( 0 == status )
	{
		status = pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);
	}
#endif
	if ( 0 == status )
	{
		status = pthread_mutex_init(&(sync_object->native_sync), &ma);
	}
	pthread_mutexattr_destroy(&ma);

#elif DDNS_SYNC_WINDOWS

	InitializeCriticalSection(&(sync_object->native_sync));

#endif

	return status;
}


/**
 *	Try to lock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be locked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_trylock(
	struct ddns_sync_object * sync_object )
{
	int status = 0;

#if DDNS_SYNC_UNIX

	status = pthread_mutex_trylock(&(sync_object->native_sync));

#elif DDNS_SYNC_WINDOWS

	if ( FALSE == TryEnterCriticalSection(&(sync_object->native_sync)) )
	{
		status = -1;
	}

#endif

	return status;
}


/**
 *	Lock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be locked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_lock(
	struct ddns_sync_object * sync_object )
{
	int status = 0;

#if DDNS_SYNC_UNIX

	status = pthread_mutex_lock(&(sync_object->native_sync));

#elif DDNS_SYNC_WINDOWS

	EnterCriticalSection(&(sync_object->native_sync));

#endif

	return status;
}


/**
 *	Unlock a synchronize object.
 *
 *	@param sync_object	: synchronize object to be unlocked.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_unlock(
	struct ddns_sync_object * sync_object )
{
	int status = 0;

#if DDNS_SYNC_UNIX

	status = pthread_mutex_unlock(&(sync_object->native_sync));

#elif DDNS_SYNC_WINDOWS

	LeaveCriticalSection(&(sync_object->native_sync));

#endif

	return status;
}


/**
 *	Free resources allocated for a synchronize object.
 *
 *	@param sync_object	: synchronize object to be freed.
 *
 *	@return If successful, it will return zero. Otherwise, an error number will
 *			be returned to indicate the error.
 */
int ddns_sync_destroy(
	struct ddns_sync_object * sync_object )
{
	int status = 0;

#if DDNS_SYNC_UNIX

	status = pthread_mutex_destroy(&(sync_object->native_sync));

#elif DDNS_SYNC_WINDOWS

	DeleteCriticalSection(&(sync_object->native_sync));

#endif

	return status;
}
