/*
 *	Copyright (C) 2009-2010 K.R.F. Studio.
 *
 *	$Id: ddns_winver.h 243 2010-07-13 14:10:07Z karl $
 *
 *	This file is part of 'ddns', Created by karl on 2009-02-18.
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
 *  Provide Windows API support.
 */

#ifndef _INC_WINVER
#define _INC_WINVER

#define	WINVER			0x0400
#define _WIN32_WINNT	0x0400

#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#endif	/* !defined(_INC_WINVER) */
