/*
 *	Copyright (C) 2008-2010 K.R.F. Studio.
 *
 *	$Id: main.c 295 2013-06-18 06:32:24Z kuang $
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

#ifdef HAVE_CONFIG_H
#	include "config.h"
#endif
#include "main.h"
#include <stdlib.h>		/* exit, atol                            */
#include <stdio.h>		/* getchar, printf, fopen, fgets, fclose */
#include <string.h>		/* strchr                                */
#include <assert.h>		/* assert                                */
#include <signal.h>		/* signal                                */
#include <ctype.h>		/* isspace                               */
#include "service.h"	/* ddns_nt_service                       */
#include "ddns_string.h"/* c99_strncpy                           */
#include "http.h"		/* HTTP_SUPPORT_SSL                      */

#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
#	include <openssl/crypto.h>
#endif

#if (defined(HAVE_CONFIG_H) && HAVE_GETCH)
#	include <conio.h>
#elif !defined(HAVE_CONFIG_H) && defined(WIN32)
#	include <conio.h>
#else
#	include <termios.h>
#	include <unistd.h>
#endif

#ifdef ENABLE_MULTI_DOMAIN
#error Multi-domain support is not implemented yet.
#endif

/* turn off globing for MinGW32, since we need to get "*" as command line argument */
#if defined(WIN32) && defined(__MINGW32__)
int _CRT_glob = 0;
#endif


/**
 *	Parse configuration directive.
 *
 *	@param[in]	buffer	: the configuration directive to be parsed.
 *	@param[out]	name	: name of the configuration directive
 *	@param[out]	value	: value of the configuration directive
 *
 *	@return		Return non-zero on success. Otherwise (error or comment line)
 *				return 0.
 */
static int handle_config_parseline(char * buffer, char ** name, char ** value);


/**
 *	DDNS context
 */
static struct ddns_context ddns_ctx;

#if (defined(HAVE_CONFIG_H) && (!defined(HAVE_GETCH) || 0 == HAVE_GETCH) ) || \
	(!defined(HAVE_CONFIG_H) && !defined(WIN32))
static int getch(void)
{
	int ch;
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}
#endif

#ifndef WIN32

/**
 *	Handle Unix/Linux signals
 *
 *	@param[in]	code	: signal code.
 *				- SIGINT	: interrupt program.
 *				- SIGQUIT	: quit program.
 *				- SIGKILL	: kill program.
 *				- SIGABRT	: abort program.
 *				- SIGPIPE	: write on a pipe with no reader.
 *				- ... 		: (see man pages for more)
 *
 *	@note	SIGTERM must be handled in daemon mode under Mac OS X.
 */
void handle_signals(int code)
{
	switch ( code )
	{
	case SIGINT:
	case SIGABRT:
	case SIGKILL:
	case SIGTERM:
		ddns_sync_lock(&(ddns_ctx.sync_object));
		ddns_ctx.exit_signal = 1;
		ddns_sync_unlock(&(ddns_ctx.sync_object));
		return;

	case SIGPIPE:
		/* Caught SIGPIPE, ignore it. */
		return;

	default:
		/* take default action for other signals */
		break;
	}

	exit(3);
}

#else	/* <=> #ifdef WIN32 */

/**
 *	Handle Windows signals.
 *
 *	@param[in]	dwCtrlType	: The type of control signal.
 *				- CTRL_C_EVENT			: A CTRL+C signal was received.
 *				- CTRL_BREAK_EVENT		: A CTRL+BREAK signal was received.
 *				- CTRL_CLOSE_EVENT		: User closes the console.	
 *				- CTRL_LOGOFF_EVENT		: A user is logging off
 *				- CTRL_SHUTDOWN_EVENT	: The system is shutting down.
 *
 *	@return	If the function handles the control signal, it should return TRUE.
 *			If it returns FALSE, the next handler function in the list of
 *			handlers for this process is used.
 */
BOOL WINAPI handle_signals(DWORD dwCtrlType)
{
	switch ( dwCtrlType )
	{
	case CTRL_C_EVENT:
	case CTRL_BREAK_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		ddns_sync_lock(&(ddns_ctx.sync_object));
		ddns_ctx.exit_signal = 1;
		ddns_sync_unlock(&(ddns_ctx.sync_object));
		return TRUE;
		break;

	default:
		break;
	}

	return FALSE;
}

#endif	/* WIN32 */

/**
 *	main entry of the program, parse command line parameters here.
 *
 *	@param[in]	argc	: count of arguments.
 *	@param[in]	argv	: argument list.
 *
 *	@return	return 0 in normal condition, return non-zero on errors.
 */
int main(int argc, const char * argv[])
{
	int i = 0;
	int set_verbose_mode = 0;

#ifdef WIN32
	int nt_service_mode = 0;
#endif
#ifdef ENABLE_DAEMON_MODE
	int daemon_mode = 0;
#endif

	ddns_initcontext(&ddns_ctx);
	ddns_ctx.interval		= -1;
	ddns_ctx.timeout		= -1;
	ddns_ctx.auto_restart	= -1;

#ifdef WIN32
	SetConsoleCtrlHandler(&handle_signals, TRUE);
#else
	signal(SIGINT, &handle_signals);
	signal(SIGPIPE, &handle_signals);
	signal(SIGKILL, &handle_signals);
	signal(SIGTERM, &handle_signals);
#endif

	if ( argc <= 1 )
	{
		print_usage();
		goto _DONE;
	}
	else if ( 0 == strcmp("-h", argv[1]) ||
			  0 == strcmp("--help", argv[1]) )
	{
		handle_help(argc - 1, argv + 1);
		goto _DONE;
	}
	else if ( 0 == strcmp("-V", argv[1]) ||
			  0 == strcmp("--version", argv[1]) )
	{
		print_copyright();
		goto _DONE;
	}
#ifdef WIN32
	else if ( 0 == strcmp("--install", argv[1]) )
	{
		const char * service_name	= "DDNS";
		const char * config_file	= NULL;
		if ( argc >= 3 )
		{
			service_name = argv[2];
		}
		if ( argc >= 4 )
		{
			config_file = argv[3];
		}
		ddns_nt_service_install(&ddns_ctx, service_name, config_file, NULL, NULL);
		goto _DONE;
	}
#endif

	for ( i = 1 ; i < argc; ++i )
	{
		if ( 0 == strcmp("-c", argv[i]) ||
			 0 == strcmp("--config", argv[i]) )
		{
			int result = handle_config(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( 0 == strcmp("-p", argv[i]) ||
				  0 == strcmp("--protocol", argv[i]) )
		{
			int result = handle_protocol(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( 0 == strcmp("-s", argv[i]) ||
				  0 == strcmp("--server", argv[i]) )
		{
			int result = handle_server(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( argc > i + 1 && ( 0 == strcmp("-d", argv[i]) ||
									0 == strcmp("--domain", argv[i])) )
		{
			int result = handle_domain(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( 0 == strcmp("-t", argv[i]) ||
				  0 == strcmp("--timeout", argv[i]) )
		{
			int result = handle_timeout(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( 0 == strcmp("-v", argv[i]) ||
				  0 == strcmp("--verbose", argv[i]) )
		{
			if ( set_verbose_mode )
			{
				ddns_msg(&ddns_ctx, msg_type_error, "Option -v and -q can't be used together.\n");
				goto _DONE;
			}
			set_verbose_mode		= 1;
			ddns_ctx.verbose_mode	= verbose_verbose;
		}
		else if ( 0 == strcmp("-q", argv[i]) ||
				  0 == strcmp("--quiet", argv[i]) )
		{
			if ( set_verbose_mode )
			{
				ddns_msg(&ddns_ctx, msg_type_error, "Option -v and -q can't be used together.\n");
				goto _DONE;
			}
			set_verbose_mode		= 1;
			ddns_ctx.verbose_mode	= verbose_quiet;
		}
		else if ( 0 == strcmp("-i", argv[i]) ||
				  0 == strcmp("--interval", argv[i]) )
		{
			int result = handle_interval(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
#ifdef ENABLE_DAEMON_MODE
		else if ( 0 == strcmp("-m", argv[i]) ||
				  0 == strcmp("--daemon", argv[i]) )
		{
			daemon_mode = 1;
		}
#endif
#ifdef WIN32
		else if ( 0 == strcmp("--service", argv[i]) )
		{
			nt_service_mode = 1;
		}
#endif
		else if ( 0 == strcmp("-a", argv[i]) ||
				  0 == strcmp("--auto-restart", argv[i]) )
		{
			int result = handle_restart(&ddns_ctx, argc - i, argv + i);
			if ( result < 0 )
				goto _DONE;
			i += (result - 1);
		}
		else if ( 0 == strcmp("-l", argv[i]) ||
				  0 == strcmp("--log", argv[i]) )
		{
			int result = handle_log(&ddns_ctx, argc - i, argv + i);
			if (result < 0)
				goto _DONE;
			i += (result - 1);
		}
		else if (  argc > i + 1 && argv[i][0] != '-' && argv[i+1][0] != '-' )
		{
			c99_strncpy(ddns_ctx.username, argv[i++], _countof(ddns_ctx.username));
			if ( strcmp("*", argv[i]) )
			{
				c99_strncpy(ddns_ctx.password, argv[i], _countof(ddns_ctx.password));
			}
			else
			{
				printf("Password: ");
				getpasswd(ddns_ctx.password, _countof(ddns_ctx.password));
			}
		}
		else
		{
			ddns_msg(&ddns_ctx, msg_type_warning, "Unknown option: \"%s\"\n", argv[i]);
		}
	}

	/* provide defaults for optional options */
	fill_default_options(&ddns_ctx);

	/* check critical options, exit on any error. */
	if ( !check_critical_options(&ddns_ctx) )
	{
		print_usage();
		goto _DONE;
	}

#if defined(ENABLE_DAEMON_MODE) && !defined(WIN32)
	if ( 0 != daemon_mode )
	{
		daemon(0, 0);
	}
#endif

#ifndef WIN32
	ddns_execute(&ddns_ctx);
#else
	if ( 0 != nt_service_mode )
	{
		if ( 0 != ddns_nt_service(&ddns_ctx) )
		{
			ddns_msg(&ddns_ctx, msg_type_error, "Incorrect usage of option \"--service\".\n");
		}
	}
	else
	{
		ddns_execute(&ddns_ctx);
	}
#endif

_DONE:
	ddns_clearcontext(&ddns_ctx);

	return 0;
}


/**
 *	Output copyright strings to stdout.
 */
void print_copyright(void)
{
	printf("DDNS client tool, v1.0-r%s", ddns_getver());
#if defined(HTTP_SUPPORT_SSL_OPENSSL) && HTTP_SUPPORT_SSL_OPENSSL
	printf(", %s", SSLeay_version(SSLEAY_VERSION));
#endif
	printf(".\n(C) 2001-2013 K.R.F. Studio. All rights reserved.\n");
	printf(" * E-mail   : master@a1983.com.cn\n");
	printf(" * Homepage : http://blog.a1983.com.cn\n\n");
}


/**
 *	Output help messages to stdout.
 */
void print_usage(void)
{
	printf(	"Usage:\n"
			"    ddns --version\n"
			"    ddns --help [subjects]\n"
#ifdef WIN32
			"    ddns --install [service name][ configuration file]\n"
#endif
			"    ddns [options] username {password|*}\n"
			"\n"
			"    -V, --version   Show version.\n"
			"    -h, --help      Show this information.\n"
			"    username        Username of the account to login to ddns server.\n"
			"    password        Password of the account.\n"
			"\n"
			"Help Subjects:\n"
			"    protocols       List all available DDNS protocols.\n"
			"\n"
			"Options:\n"
			"    -c, --config    Path of configuration file.\n"
			"    -l, --log       Path of log file.\n"
			"    -p, --protocol  DDNS protocol type, default is peanuthull.\n"
			"    -s, --server    DDNS server address in \"domain:port\" favor.\n"
			"    -d, --domain    The domain name you wish to update.\n"
			"    -a, --auto-restart\n"
			"                    auto restart interval at failure, default = 60s.\n"
			"                    Exception: 0 = no auto restart.\n"
			"\n"
			"                        NOTE: '-s' and '-d' option may appear more than once.\n"
			"\n"
			"    -v, --verbose   turn on verbose mode.\n"
			"    -q, --quiet     turn on quiet mode.\n"
			"\n"
			"                        NOTE: '-v' and '-q' can't be used together.\n"
			"\n"
			"    -t, --timeout   DDNS server timeout in seconds, default = 15s.\n"
			"    -i, --interval  DDNS keep-alive interval, default = 60s.\n"
#ifdef ENABLE_DAEMON_MODE
			"    -m, --daemon    Launch the tool in daemon mode.\n"
#endif
#ifdef WIN32
			"    --service       Launch the tool as Windows service.\n"
#endif
			"\n"
		);
}


/**
 *	List available protocols to stdout
 */
void print_protocols(void)
{
	printf(	"Supported DDNS protocols:\n"
#ifndef DISABLE_PEANUTHULL
			"    peanuthull  - Oray.cn\n"
#endif
#ifndef DISABLE_DYNDNS
			"    dyndns      - DynDNS.org\n"
#endif
#ifndef DISABLE_DNSPOD
			"    dnspod      - DNSPod.cn\n"
#endif
			"\n"
		);
	print_copyright();
}


/**
 *	Handle help requests.
 *
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-h".
 */
int handle_help(int argc, const char * argv[])
{
	if ( (1 < argc) && (0 == strcmp(argv[1], "protocols")) )
	{
		print_protocols();
	}
	else
	{
		print_usage();
	}
	
	return 0;
}


/**
 *	Handles configuration argument (-c, --config). Read default configurations
 *	into DDNS context.
 *
 *	@note	Command line options will override options specified in
 *			configuration file.
 *
 *	@param[in]	context		: the DDNS context.
 *	@param[in]	argc		: count of arguments in argv.
 *	@param[in]	argv		: arguments to be parsed, the first one is "-c".
 *
 *	@return	count of arguments have been eaten by this routine when every thing
 *			is going fine, otherwise -1 will be returned.
 */
int handle_config(struct ddns_context * context, int argc, const char* argv[])
{
	int			result	= 2;
	FILE	*	fconfig = NULL;
	char		buffer[1024];

	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "no configuration file is specified.\n");
		print_usage();
		return -1;
	}

	fconfig = fopen(argv[1], "r");
	if ( NULL == fconfig )
	{
		ddns_msg(context, msg_type_error, "can't read configuration file \"%s\".\n", argv[1]);
		print_usage();
		return -1;
	}

	while ( 1 )
	{
		char * name = NULL;
		char * value = NULL;

		memset(buffer, 0, sizeof(buffer));

		/* read one line from configuration file */
		if ( NULL == fgets(buffer, sizeof(buffer) - 1, fconfig) )
		{
			break;
		}

		if ( 0 == handle_config_parseline(buffer, &name, &value) )
		{
			/* error or comment line */
			continue;
		}

		if ( 0 == ddns_strcasecmp("username", name) )
		{
			if ( '\0' == context->username[0] )
			{
				c99_strncpy(context->username, value, _countof(context->username));
			}
		}
		else if ( 0 == ddns_strcasecmp("password", name) )
		{
			if ( '\0' == context->password[0] )
			{
				c99_strncpy(context->password, value, _countof(context->password));
			}
		}
		else if ( 0 == ddns_strcasecmp("protocol", name) )
		{
			if ( 0 == ddns_strcasecmp("peanuthull", value) )
			{
				context->protocol = proto_peanuthull;
			}
			else if ( 0 == ddns_strcasecmp("dnspod", value) )
			{
				context->protocol = proto_dnspod;
			}
			else if ( 0 == ddns_strcasecmp("dyndns", value) )
			{
				context->protocol = proto_dyndns;
			}
			else
			{
				ddns_msg(context, msg_type_error, "unknown DDNS protocol: '%s'.\n", value);
				result = -1;
				break;
			}
		}
		else if ( 0 == ddns_strcasecmp("domain", name) )
		{
			struct ddns_server domain;

			memset(&domain, 0, sizeof(domain));

			c99_strncpy(domain.domain, value, _countof(domain.domain));
			if ( 0 == ddns_adddomain(context, &domain) )
			{
				ddns_msg(context, msg_type_error, "insufficient memory.\n");
				result = -1;
				break;
			}
		}
		else if (0 == ddns_strcasecmp("LogFile", name))
		{
			const char * args[] = { "--log", value };
			if (2 != handle_log(context, 2, args))
			{
				return -1;
			}
		}
		else
		{
			ddns_msg(context, msg_type_warning, "unknown option \"%s\".\n", name);
		}
	}

	fclose(fconfig);

	return result;
}


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
int handle_protocol(struct ddns_context * context, int argc, const char* argv[])
{
	int continue_compare = 1;

	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "no protocol is specified.\n");
		print_usage();
		return -1;
	}

	if ( proto_unknown != context->protocol )
	{
		ddns_msg(context, msg_type_error, "multiple protocol is specified!\n");
		print_usage();
		return -1;
	}

#ifndef DISABLE_PEANUTHULL
	if ( continue_compare && 0 == strcmp("peanuthull", argv[1]) )
	{
		continue_compare = 0;
		context->protocol = proto_peanuthull;
	}
#endif

#ifndef DISABLE_DYNDNS
	if ( continue_compare && 0 == strcmp("dyndns", argv[1]) )
	{
		continue_compare = 0;
		context->protocol = proto_dyndns;
	}
#endif

#ifndef DISABLE_DNSPOD
	if ( continue_compare && 0 == strcmp("dnspod", argv[1]) )
	{
		continue_compare = 0;
		context->protocol = proto_dnspod;
	}
#endif

	if ( continue_compare )
	{
		ddns_msg(context, msg_type_error, "unknown DDNS protocol: '%s'.\n", argv[1]);
		print_usage();
		return -1;
	}

	return 2;
}


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
int handle_server(struct ddns_context * context, int argc, const char* argv[])
{
	struct ddns_server server;

	assert( context != 0 );

	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "No server is specified.\n");
		print_usage();
		return -1;
	}

	memset(&server, 0, sizeof(server));
	if ( parsedomain(argv[1],
					 server.domain,
					 _countof(server.domain),
					 &server.port) &&
		 server.domain[0] )
	{
		if ( !ddns_addserver(context, &server) )
		{
			ddns_msg(context, msg_type_error, "Insufficient memory.\n");
			return -1;
		}
	}
	else
	{
		ddns_msg(context, msg_type_error, "Invalid server '%s'.\n", argv[1]);
		return -1;
	}

	return 2;
}


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
int handle_domain(struct ddns_context * context, int argc, const char* argv[])
{
	assert( context != 0 && argv != 0 );

	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "No domain is specified.\n");
		print_usage();
		return -1;
	}
	else
	{
		struct ddns_server server;

		memset(&server, 0, sizeof(server));
		if ( parsedomain(argv[1],
						 server.domain,
						 _countof(server.domain),
						 &server.port) &&
			 server.domain[0] )
		{
			if ( !ddns_adddomain(context, &server) )
			{
				ddns_msg(context, msg_type_error, "Insufficient memory.\n");
				return -1;
			}
		}
		else
		{
			ddns_msg(context, msg_type_error, "Invalid domain '%s'.\n", argv[1]);
			return -1;
		}
	}

	return 2;
}


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
int handle_timeout(struct ddns_context * context, int argc, const char* argv[])
{
	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "No timeout is specified.\n");
		print_usage();
		return -1;
	}
	else
	{
		context->timeout = parsetime(argv[1]);
	}

	return 2;
}


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
int handle_interval(struct ddns_context * context, int argc, const char* argv[])
{
	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "No interval is specified.\n");
		print_usage();
		return -1;
	}
	else
	{
		context->interval = parsetime(argv[1]);
	}

	return 2;
}


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
int handle_restart(struct ddns_context *context, int argc, const char* argv[])
{
	if ( argc < 2 || '-' == argv[1][0] )
	{
		ddns_msg(context, msg_type_error, "No auto restart interval specified.\n");
		print_usage();
		return -1;
	}
	else
	{
		context->auto_restart = parsetime(argv[1]);
	}

	return 2;
}


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
int handle_log(struct ddns_context * context, int argc, const char * argv[])
{
	if (argc < 2 || '-' == argv[1][0])
	{
		ddns_msg(context, msg_type_error, "No log file specified.\n");
		print_usage();
		return -1;
	}
	else
	{
		ddns_ctx.stream_out = fopen(argv[1], "a");
		if (NULL == ddns_ctx.stream_out)
		{
			ddns_msg(context, msg_type_error, "couldn't open log file for writing.\n");
			print_usage();
			return -1;
		}
	}

	return 2;
}


/**
 *	Provide default options.
 *
 *	@param[in/out]	context	: the DDNS context.
 */
void fill_default_options(struct ddns_context * context)
{
	/* default protocol is 'peanuthull' */
	if ( proto_unknown == context->protocol )
	{
#if !defined(DISABLE_PEANUTHULL)
		context->protocol = proto_peanuthull;
		ddns_printf_v(context,	msg_type_info,
								"INFO: DDNS protocol default to peanuthull.\n");
#elif !defined(DISABLE_DNSPOD)
		context->protocol = proto_dnspod;
		ddns_printf_v(context,	msg_type_info,
								"INFO: DDNS protocol default to dnspod.\n");
#elif !defined(DISABLE_DYNDNS)
		context->protocol = proto_dyndns;
		ddns_printf_v(context,	msg_type_info,
								"INFO: DDNS protocol default to dyndns.\n");
#endif
	}

	if ( 0 == context->server )
	{
		struct ddns_server server;
		switch ( context->protocol )
		{
		/* for protocol 'peanuthull', default server is 'ph051.oray.net:6060' */
		case proto_peanuthull:
			server.next = NULL;
			server.port = 6060;
			c99_strncpy(server.domain, "ph051.oray.net", _countof(server.domain));
			ddns_addserver(context, &server);
			ddns_printf_v(context,	msg_type_info,
									"INFO: Default server '%s:%d' is used.\n",
									server.domain, server.port );
			break;

		/* for protocol 'dnspod', default server is 'www.dnspod.com:443' */
		case proto_dnspod:
			server.next = NULL;
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
			server.port = 443;
#else
			server.port = 80;
#endif
			c99_strncpy(server.domain, "dnsapi.cn", _countof(server.domain));
			ddns_addserver(context, &server);
			ddns_printf_v(context,	msg_type_info,
									"INFO: Default server '%s:%d' is used.\n",
									server.domain, server.port );
			break;

		/* for protocol 'dyndns', default server is 'members.dyndns.org:443' */
		case proto_dyndns:
			server.next = NULL;
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
			server.port = 443;
#else
			server.port = 80;
#endif
			c99_strncpy(server.domain, "members.dyndns.org", _countof(server.domain));
			ddns_addserver(context, &server);
			ddns_printf_v(context,	msg_type_info,
									"INFO: Default server '%s:%d' is used.\n",
									server.domain, server.port );
			break;

		case proto_unknown:
		default:
			break;
		}
	}
	else if ( 0 == context->server->port )
	{
		switch ( context->protocol )
		{
		case proto_peanuthull:
			context->server->port = 6060;
			break;

		case proto_dnspod:
		case proto_dyndns:
#if defined(HTTP_SUPPORT_SSL) && HTTP_SUPPORT_SSL
			context->server->port = 443;
#else
			context->server->port = 80;
#endif
			break;

		case proto_unknown:
		default:
			break;
		}
	}

	/* default connect timeout is 15s */
	if ( context->timeout <= 0 )
	{
		context->timeout = 15;
		ddns_printf_v(context,	msg_type_info,
								"INFO: Timeout default to 15s.\n" );
	}

	/* default keep alive interval is 60s */
	if ( context->interval <= 0 )
	{
		context->interval = 60;
		ddns_printf_v(context,	msg_type_info,
								"INFO: Interval default to 60s.\n" );
	}

	/* default auto restart interval is 60s */
	if ( context->auto_restart < 0 )
	{
		context->auto_restart = 60;
		ddns_printf_v(context,	msg_type_info,
								"INFO: Auto restart interval default to 60s.\n" );
	}
}


/**
 *	Check if all critical options are provided.
 *
 *	@param[in]	context		: the DDNS context.
 *
 *	@return	non-zero if every thing is OK otherwise 0.
 */
int check_critical_options(struct ddns_context * context)
{
	if ( proto_unknown == context->protocol )
	{
		ddns_msg(context, msg_type_error, "no DDNS protocol is specified.\n");
		return 0;
	}

	if ( 0 == context->server )
	{
		ddns_msg(context, msg_type_error, "at least one server is required.\n");
		return 0;
	}
	else if ( 0 == context->server->port )
	{
		ddns_msg(context, msg_type_error, "server port is not specified.\n");
		return 0;
	}

	if ( 0 == context->username[0] || 0 == context->password[0] )
	{
		ddns_msg(context, msg_type_error, "both username and password are required.\n");
		return 0;
	}

	return 1;
}


/**
 *	Read a string from stdin and show the string as "***"
 *
 *	@param[out]	buffer	: buffer to store characters entered by user.
 *	@param[in]	size	: size of the output buffer in bytes.
 *
 *	@return	count of the characters entered by user.
 */
int getpasswd(char *buffer, int size)
{
	int j = 0, end = 0;

	memset(buffer, 0, size * sizeof(buffer[0]));
	for ( ; j < size - 1 && 0 == end; ++j )
	{
		buffer[j] = (char)getch();
		switch( buffer[j] )
		{
		case '\r':
		case '\n':
			buffer[j] = '\0';
			end = 1;
			break;

		case '\x7f':
		case '\x8':		/* backspace */
			buffer[j--] = '\0';
			if ( j >= 0 )
			{
				printf("\x8 \x8");
				buffer[j--] = '\0';
			}
			break;

		default:
			printf("*");
			break;
		}
	}
	printf("\n");

	return j;
}


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
int parsedomain(const char * addr, char * domain, int len, unsigned short * port)
{
	int i = 0;
	if ( 0 == addr )
	{
		return 0;
	}

	memset(domain, 0, len);
	for ( i = 0; addr[i] && i < len - 1; ++i )
	{
		if ( (addr[i] >= 'a' && addr[i] <= 'z') ||
			 (addr[i] >= 'A' && addr[i] <= 'Z') ||
			 (addr[i] >= '0' && addr[i] <= '9') ||
			 addr[i] == '-' ||
			 addr[i] == '.' )
		{
			domain[i] = addr[i];
		}
		else
		{
			break;
		}
	}

	if ( ':' == addr[i] )
	{
		int tmp = 0;
		for ( ++i; addr[i] && i < len - 1; ++i )
		{
			if ( addr[i] >= '0' && addr[i] <= '9' )
			{
				tmp = tmp * 10 + addr[i] - '0';
				if ( tmp > ((unsigned short)(signed short)-1) )
				{
					return 0;
				}
			}
			else
			{
				return 0;
			}
		}
		if ( tmp < 0 || tmp > ((unsigned short)(signed short)-1) || addr[i] )
		{
			return 0;
		}
		else if ( port )
		{
			*port = (unsigned short)tmp;
		}
	}
	
	return 1;
}


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
int parsetime(const char* interval)
{
	int i = 0, value = -1, digit = 0, factor = 1;
	for ( ; interval && interval[i]; ++i )
	{
		if ( '0' <= interval[i] && interval[i] <= '9' )
		{
			digit = interval[i] - '0';
			if ( -1 == value )
			{
				value = digit;
			}
			else
			{
				value = value * 10 + digit;
			}
		}
		else
		{
			switch( interval[i] )
			{
			case 's':
			case 'S':
				break;

			case 'm':
			case 'M':
				factor = 60;
				break;

			case 'h':
			case 'H':
				factor = 3600;
				break;

			case 'd':
			case 'D':
				factor = 3600 * 24;
				break;

			default:
				factor = -1;
			}

			if ( -1 != factor )
			{
				++i;
				if ( -1 != value )
				{
					value *= factor;
				}
			}

			break;
		}
	}

	if ( interval && '\0' != interval[i] )
	{
		value = -1;
	}

	return value;
}


/**
 *	Parse configuration directive.
 *
 *	@param[in]	buffer	: the configuration directive to be parsed.
 *	@param[out]	name	: name of the configuration directive
 *	@param[out]	value	: value of the configuration directive
 *
 *	@return		Return non-zero on success. Otherwise (error or comment line)
 *				return 0.
 */
static int handle_config_parseline(char * buffer, char ** name, char ** value)
{
	static const int RESULT_SUCCESS = 1;
	static const int RESULT_FAILURE = 0;
	
	int idx				= 0;
	int equal_erased	= 0;
	
	/* skip leading spaces */
	for( idx = 0; '\0' != buffer[idx]; ++idx )
	{
		if ( 0 == isspace(buffer[idx]) )
		{
			break;
		}
	}
	
	/* skip current line if it's comments line */
	if ( NULL != strchr("#;", buffer[idx]) )
	{
		return RESULT_FAILURE;
	}
	
	/* get option name */
	if ( NULL != name )
	{
		(*name) = &(buffer[idx]);
	}
	for ( ; '\0' != buffer[idx]; ++idx )
	{
		if ( (0 != isspace(buffer[idx])) || ('=' == buffer[idx]) )
		{
			if ( '=' == buffer[idx] )
			{
				equal_erased = 1;
			}
			buffer[idx] = '\0';
			++idx;
			break;
		}
	}
	
	/* skip spaces between option name and "=" */
	for ( ; '\0' != buffer[idx]; ++idx )
	{
		if ( 0 == isspace(buffer[idx]) )
		{
			break;
		}
	}
	
	/* skip "=" between option name and option value */
	if ( 0 == equal_erased )
	{
		if ( ('=' != buffer[idx]) )
		{
			/* "=" is expected here. */
			return RESULT_FAILURE;
		}
		else if ( '\0' != buffer[idx] )
		{
			++idx;
		}
	}
	
	/* skip spaces between "=" and option value */
	for ( ; '\0' != buffer[idx]; ++idx )
	{
		if ( 0 == isspace(buffer[idx]) )
		{
			break;
		}
	}
	
	/* now we reach the option value */
	if ( NULL != value )
	{
		(*value) = &(buffer[idx]);
	}
	
	/* remove the trailing '\n' */
	for ( ; '\0' != buffer[idx]; ++idx )
	{
		if ( ('\n' == buffer[idx]) || ('\r' == buffer[idx]) )
		{
			buffer[idx] = '\0';
		}
	}
	
	return RESULT_SUCCESS;
}
