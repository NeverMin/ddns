/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: main.h 293 2013-03-07 10:11:34Z kuang $
 *
 *	This file is part of 'ddns', Created by karl on 2008-03-03.
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
 *  Program entry of ddns tool.
 */

#ifndef _INC_DDNS_MAIN
#define _INC_DDNS_MAIN

#include "ddns.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Main entry of the program.
 */
int main(int argc, const char * argv[]);


/**
 *	Output copyright strings to stdout.
 */
void print_copyright(void);


/**
 *	Output help messages to stdout.
 */
void print_usage(void);


/**
 *	List available protocols to stdout
 */
void print_protocols(void);


/**
 *	Handle help requests.
 *
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-h".
 */
int handle_help(int argc, const char * argv[]);


/**
 *	Handles configuration argument (-c, --config). Read default configurations
 *	into DDNS context.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-c".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_config(struct ddns_context * context, int argc, const char* argv[]);


/**
 *	Handles DDNS protocol argument (-p, --protocol).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-p".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_protocol(struct ddns_context *context, int argc, const char* argv[]);


/**
 *	Handles DDNS server argument (-s, --server).
 *
 *	@param[in]	context		: DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-s".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_server(struct ddns_context *context, int argc, const char* argv[]);


/**
 *	Handles domain argument (-d, --domain).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-d".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_domain(struct ddns_context *context, int argc, const char* argv[]);


/**
 *	Handles connect timeout argument (-t, --timeout).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-t".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_timeout(struct ddns_context *context, int argc, const char* argv[]);


/**
 *	Handles keep-alive interval argument (-i, --interval).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-i".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_interval(struct ddns_context * context, int argc, const char* argv[]);


/**
 *	Handles restart interval argument (-a, --auto-restart).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-a".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_restart(struct ddns_context *context, int argc, const char* argv[]);


/**
 *	Handles log file argument (-l, --log).
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-l".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_log(struct ddns_context * context, int argc, const char * argv[]);


/**
 *	Provide default options.
 *
 *	@param[in/out]	context	: the DDNS context.
 */
void fill_default_options(struct ddns_context *context);


/**
 *	Check if all critical options are provided.
 *
 *	@param[in]	context		: the DDNS context.
 *
 *	@return	non-zero if every thing is OK otherwise 0.
 */
int check_critical_options(struct ddns_context *context);


/**
 *	Read a string from stdin and show the string as "***"
 *
 *	@param[out]	buffer	: buffer to store characters entered by user.
 *	@param[in]	size	: size of the output buffer in bytes.
 *
 *	@return	count of the characters entered by user.
 */
int getpasswd(char *buffer, int size);


/**
 *	Parse domain argument.
 *
 *	@param[in]	addr	: domain address, in "domain:port" favor.
 *	@param[out]	domain	: formatted domain name.
 *	@param[in]	len		: size of the domain name buffer in bytes.
 *	@param[out]	port	: port of the address.
 *
 *	@return	If 'addr' is a valid address, non-zero will be returned. If addr is
 *			ill formatted, 0 will be returned.
 */
int parsedomain(const char* addr, char *domain, int len, unsigned short *port);


/**
 *	Parse time argument.
 *
 *	@param[in]	interval	: C-style string of the interval. Various interval
 *							  formats like "10", "10s", "10m", "10h", "10d" can
 *							  be accepted. The default time unit is second.
 *
 *	@return	If interval is in correct form, interval in second will be returned.
 *			Otherwise, -1 will be returned.
 */
int parsetime(const char* interval);


#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif	/* _INC_DDNS_MAIN */
