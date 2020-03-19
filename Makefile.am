#
#  Copyright (C) 2008-2010 K.R.F. Studio.
#
#  $Id: Makefile.am 296 2014-01-21 03:27:55Z kuang $
#
#  This file is part of 'ddns', Created by karl on 2008-03-26.
#
#  'ddns' is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published
#  by the Free Software Foundation; either version 3 of the License,
#  or (at your option) any later version.
#
#  'ddns' is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with 'ddns'; if not, write to the Free Software Foundation, Inc.,
#  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. 
#

AUTOMAKE_OPTIONS=foreign

.rc.o:
	$(RC) -o $@ $<

CFLAGS+=-DHAVE_REPOSITORY_REVISION
FORCE:
all: ddns_version.h
ddns_version.h: FORCE
	./version.sh

bin_PROGRAMS= ddns

if enable_debug
CFLAGS += -DDEBUG
else
CFLAGS += -DNDEBUG
endif

if enable_daemon_mode
CFLAGS += -DENABLE_DAEMON_MODE
endif

if enable_ip_dnspod
CFLAGS += -DDNSPOD_USE_DNSPOD=1
else
CFLAGS += -DDNSPOD_USE_DNSPOD=0
endif

if enable_ip_baidu
CFLAGS += -DDNSPOD_USE_BAIDU=2
else
CFLAGS += -DDNSPOD_USE_BAIDU=0
endif

if enable_ip_ip138
CFLAGS += -DDNSPOD_USE_IP138=3
else
CFLAGS += -DDNSPOD_USE_IP138=0
endif

if !want_peanuthull
CFLAGS += -DDISABLE_PEANUTHULL
endif
if !want_dyndns
CFLAGS += -DDISABLE_DYNDNS
endif
if !want_dnspod
CFLAGS += -DDISABLE_DNSPOD
endif

ddns_SOURCES = main.c ddns_string.c ddns.c ddns_sync.c ddns_socket.c
if enable_service
ddns_SOURCES += service.c
endif
if want_resource
ddns_SOURCES += resource.rc
endif

# if http.c is needed
if want_dyndns
ddns_SOURCES += http.c
else
if want_dnspod
ddns_SOURCES += http.c
endif
endif

if want_peanuthull
ddns_SOURCES += oraypeanut.c blowfish.c hmac.c base64.c md5.c sha1.c
endif

if want_dnspod
ddns_SOURCES += dnspod.c json.c
endif

if want_dyndns
ddns_SOURCES += dyndns.c
endif

